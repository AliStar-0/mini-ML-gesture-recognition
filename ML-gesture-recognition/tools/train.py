import argparse
import sys
from pathlib import Path

import numpy as np
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.neural_network import MLPClassifier

ROOT = Path(__file__).resolve().parent.parent
N_STEPS = 50
N_CHANNELS = 6
N_FEATURES = N_STEPS * N_CHANNELS

INPUT_VERSION = 2


def load_data(data_dir):
    labels, xs, ys = [], [], []
    for path in sorted(data_dir.glob("*.csv")):
        rows = np.loadtxt(path, delimiter=",", ndmin=2)
        if rows.shape[1] != N_FEATURES:
            sys.exit(f"{path.name}: expected {N_FEATURES} columns, found {rows.shape[1]}.")
        labels.append(path.stem)
        xs.append(rows)
        ys.append(np.full(len(rows), len(labels) - 1))
        print(f"  {path.stem:<12} {len(rows)} gestures")

    if len(labels) < 2:
        sys.exit(f"Need at least 2 gestures in {data_dir}. Record some with tools/collect.py.")
    smallest = min(len(x) for x in xs)
    if smallest < 8:
        sys.exit("Every gesture needs at least 8 recordings (30 or more is better).")
    if smallest < 20:
        print("  Note: under 20 recordings for some gestures. More data will help.")
    return labels, np.vstack(xs), np.concatenate(ys)


def augment(x, y, copies, rng):
    out_x, out_y = [x], [y]
    steps = np.arange(N_STEPS)
    last = N_STEPS - 1
    for _ in range(copies):
        g = x.reshape(-1, N_STEPS, N_CHANNELS).copy()
        n = len(g)

        start = rng.uniform(-0.12, 0.12, size=n)
        end = rng.uniform(0.88, 1.12, size=n)
        for i in range(n):
            pos = np.clip((start[i] + (end[i] - start[i]) * steps / last) * last, 0, last)
            for ch in range(N_CHANNELS):
                g[i, :, ch] = np.interp(pos, steps, g[i, :, ch])

        mean = g.mean(axis=1, keepdims=True)
        g = mean + (g - mean) * rng.uniform(0.7, 1.4, size=(n, 1, 1))

        g += rng.normal(0, 0.02, size=g.shape)
        out_x.append(g.reshape(n, -1))
        out_y.append(y)
    return np.vstack(out_x), np.concatenate(out_y)


def prepare(x):
    g = x.reshape(-1, N_STEPS, N_CHANNELS).copy()
    g[:, :, :3] -= g[:, :, :3].mean(axis=1, keepdims=True)
    return g.reshape(len(g), -1)


def layers_for_export(model):
    layers = [(w.T.copy(), b.copy()) for w, b in zip(model.coefs_, model.intercepts_)]
    if model.out_activation_ == "logistic":
        w, b = layers[-1]
        layers[-1] = (np.vstack([np.zeros_like(w), w]), np.concatenate([[0.0], b]))
    return layers


def forward(layers, x):
    a = x
    for i, (w, b) in enumerate(layers):
        a = a @ w.T + b
        if i < len(layers) - 1:
            a = np.maximum(a, 0)
    a = np.exp(a - a.max(axis=1, keepdims=True))
    return a / a.sum(axis=1, keepdims=True)


def c_array(values, per_line=8):
    flat = np.asarray(values, dtype=np.float32).ravel()
    lines = []
    for i in range(0, len(flat), per_line):
        lines.append("  " + ", ".join(f"{v:.8e}f" for v in flat[i:i + per_line]) + ",")
    return "\n".join(lines)


def write_header(path, labels, layers):
    sizes = [layers[0][0].shape[1]] + [w.shape[0] for w, _ in layers]
    n_params = sum(w.size + b.size for w, b in layers)
    text = f"""#pragma once

#define MODEL_IS_PLACEHOLDER 0

constexpr int MODEL_INPUT_VERSION = {INPUT_VERSION};

constexpr int MODEL_N_INPUTS = {sizes[0]};
constexpr int MODEL_N_CLASSES = {sizes[-1]};
constexpr int MODEL_N_LAYERS = {len(layers)};
constexpr int MODEL_MAX_WIDTH = {max(sizes[1:])};

static const int MODEL_LAYER_SIZES[] = {{{", ".join(map(str, sizes))}}};

static const char* const MODEL_LABELS[] = {{{", ".join(f'"{name}"' for name in labels)}}};

static const float MODEL_WEIGHTS[] = {{
{chr(10).join(c_array(w) for w, _ in layers)}
}};

static const float MODEL_BIASES[] = {{
{chr(10).join(c_array(b) for _, b in layers)}
}};
"""
    path.write_text(text)
    return n_params


def save_confusion_matrix(path, matrix, labels):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed, skipping the confusion matrix image.")
        return

    fig, ax = plt.subplots(figsize=(1.2 * len(labels) + 2, 1.2 * len(labels) + 1.5))
    ax.imshow(matrix, cmap="Blues")
    ax.set_xticks(range(len(labels)), labels, rotation=45, ha="right")
    ax.set_yticks(range(len(labels)), labels)
    ax.set_xlabel("Predicted")
    ax.set_ylabel("Actual")
    ax.set_title("Confusion matrix (test set)")
    half = matrix.max() / 2
    for i in range(len(labels)):
        for j in range(len(labels)):
            ax.text(j, i, matrix[i, j], ha="center", va="center",
                    color="white" if matrix[i, j] > half else "black")
    fig.tight_layout()
    path.parent.mkdir(exist_ok=True)
    fig.savefig(path, dpi=150)
    print(f"Saved {path.relative_to(ROOT)}")


def main():
    parser = argparse.ArgumentParser(description="Train the gesture model and export it as include/model.h.")
    parser.add_argument("--data", type=Path, default=ROOT / "data")
    parser.add_argument("--out", type=Path, default=ROOT / "include" / "model.h")
    parser.add_argument("--hidden", type=int, nargs="+", default=[32, 16],
                        help="hidden layer sizes (default: 32 16)")
    parser.add_argument("--test-size", type=float, default=0.25,
                        help="share of recordings held back for testing (default 0.25)")
    parser.add_argument("--augment", type=int, default=8,
                        help="extra copies of each training gesture (default 8)")
    parser.add_argument("--seed", type=int, default=0)
    args = parser.parse_args()

    rng = np.random.default_rng(args.seed)

    print("Loading data:")
    labels, x, y = load_data(args.data)

    x_train, x_test, y_train, y_test = train_test_split(
        x, y, test_size=args.test_size, stratify=y, random_state=args.seed)
    x_aug, y_aug = augment(x_train, y_train, args.augment, rng)
    print(f"\nTraining on {len(x_train)} gestures ({len(x_aug)} after augmentation), "
          f"testing on {len(x_test)}.")

    model = MLPClassifier(hidden_layer_sizes=tuple(args.hidden), activation="relu",
                          alpha=1e-3, max_iter=2000, random_state=args.seed)
    model.fit(prepare(x_aug), y_aug)

    inputs_test = prepare(x_test)
    predicted = model.predict(inputs_test)
    accuracy = float((predicted == y_test).mean())
    print(f"\nTest accuracy: {accuracy * 100:.1f}%")

    x_varied, y_varied = augment(x_test, y_test, 10, np.random.default_rng(args.seed + 1))
    x_varied, y_varied = x_varied[len(x_test):], y_varied[len(y_test):]
    varied = float((model.predict(prepare(x_varied)) == y_varied).mean())
    print(f"Test accuracy with timing and size variations: {varied * 100:.1f}%\n")
    print(classification_report(y_test, predicted, target_names=labels, zero_division=0))
    save_confusion_matrix(ROOT / "docs" / "confusion_matrix.png",
                          confusion_matrix(y_test, predicted), labels)

    layers = layers_for_export(model)
    difference = np.abs(forward(layers, inputs_test) - model.predict_proba(inputs_test)).max()
    if difference > 1e-4:
        sys.exit(f"Export check failed (difference {difference:.2e}). model.h was not written.")

    n_params = write_header(args.out, labels, layers)
    print(f"Wrote {args.out.relative_to(ROOT) if args.out.is_relative_to(ROOT) else args.out}: "
          f"{n_params} parameters, {n_params * 4 / 1024:.1f} KB of flash.")
    print("Now upload the `recognize` firmware.")


if __name__ == "__main__":
    main()
