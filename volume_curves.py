import numpy as np
import matplotlib.pyplot as plt

# --- CONFIGURATION ---
MIN_DB = -60.0
MAX_DB = 0.0
STEPS = 32

def db_to_amp(db):
    return 10**(db / 20.0)

def amp_to_db(amp):
    # Clamp to avoid log(0)
    return 20 * np.log10(np.maximum(amp, db_to_amp(MIN_DB)))

# --- ALGORITHMS ---

def algo_linear_db(t):
    """Linear transition in decibels."""
    return MIN_DB + (t * (MAX_DB - MIN_DB))

def algo_squared(t):
    """
    Amplitude follows t^2. 
    To fit -60dB to 0dB, we map t=0..1 to the amplitude range 0.001..1.0
    """
    min_amp = db_to_amp(MIN_DB) # 0.001
    amp = t**2
    # Rescale to ensure it starts at -60dB (0.001 amp)
    scaled_amp = min_amp + (amp * (1.0 - min_amp))
    return amp_to_db(scaled_amp)

def algo_perceptual(t):
    """
    Commonly used for human hearing (Logarithmic/Exponential).
    Uses t^3 or t^4 for a very slow rise at the bottom.
    """
    min_amp = db_to_amp(MIN_DB)
    amp = t**3 # Cubic rise
    scaled_amp = min_amp + (amp * (1.0 - min_amp))
    return amp_to_db(scaled_amp)

def algo_scurve(t):
    """Smooth start and smooth finish."""
    s = t * t * (3.0 - 2.0 * t)
    return MIN_DB + (s * (MAX_DB - MIN_DB))

# --- PLOTTING & GENERATION ---

def plot_all():
    t = np.linspace(0, 1, STEPS)
    
    curves = {
        "Linear dB": algo_linear_db(t),
        "Squared (t^2)": algo_squared(t),
        "Perceptual (t^3)": algo_perceptual(t),
        "S-Curve": algo_scurve(t)
    }

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    for name, values in curves.items():
        ax1.plot(t, values, label=name)
        ax2.plot(t, 10**(values/20), label=name)

    ax1.set_title("Response in Decibels")
    ax1.set_ylabel("dB")
    ax1.set_xlabel("Input (t)")
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.set_title("Response in Amplitude (Linear Gain)")
    ax2.set_ylabel("Gain")
    ax2.set_xlabel("Input (t)")
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.show()

def generate_header():
    t = np.linspace(0, 1, STEPS)
    
    curves = [
        ("SQUARED", algo_squared(t)),
        ("PERCEPTUAL", algo_perceptual(t)),
        # ("LINEAR_DB", algo_linear_db(t)),
        # ("S_CURVE", algo_scurve(t))
    ]

    print("/* --- GENERATED VOLUME LUT HEADER --- */")
    print("#include <pgmspace.h>\n")
    print(f"static constexpr int VOL_LUT_STEPS = {STEPS};")
    print(f"static constexpr int VOL_LUT_CURVES = {len(curves)};\n")
    
    print("// Curve Indices")
    for i, (name, _) in enumerate(curves):
        print(f"static constexpr int VOL_CURVE_{name} = {i};")
    
    print(f"\nconst float VOLUME_TABLE[VOL_LUT_CURVES][VOL_LUT_STEPS] PROGMEM = {{")
    
    for name, vals in curves:
        print(f"    {{ // {name}")
        for i, v in enumerate(vals):
            # Print 8 values per line for readability
            sep = ",\n        " if (i + 1) % 8 == 0 and (i + 1) != STEPS else ", "
            if (i + 1) == STEPS: sep = ""
            print(f"{v:7.2f}f", end=sep)
        
        suffix = "}," if name != curves[-1][0] else "}"
        print(f"\n    {suffix}")
        
    print("};\n")


if __name__ == "__main__":
    plot_all()
    # Uncomment to generate C++ code:
    generate_header()