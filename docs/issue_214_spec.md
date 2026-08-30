# Issue #214 / PR #335 : GPIO on Raspberry Pi 5

## What the issue and PR are

[#214](https://github.com/benkuper/Chataigne/issues/214) : Chataigne's GPIO module doesn't see the device on a Pi 5. The vendored `pigpio` library maps the GPIO controller registers directly from `/dev/mem`, which only works on the BCM283x / BCM2711 SoCs up to the Pi 4. The Pi 5 moved GPIO to a separate RP1 chip with a different register layout, and `pigpio` (unmaintained since 2021) never learned to talk to it, so `gpioInitialise()` always fails there.

[PR #335](https://github.com/benkuper/Chataigne/pull/335) (author `dionzand`, branch `gpio-libgpiod-migration`) : deletes the vendored `pigpio` tree (~22k lines) and adds a small shim `Source/Module/modules/gpio/gpiod/ChataigneGPIO.{h,c}` (~330 lines) built on `libgpiod`, the kernel's gpiochip character-device API. It reproduces only the 5 pigpio functions `GPIOModule.cpp` actually calls, with the same names and signatures, so `GPIOModule.cpp` itself is unchanged. It also drops the need for root or a running `pigpiod` daemon (membership of the `gpio` group is enough), and bumps the `webkit2gtk` pkg-config dependency from 4.0 to 4.1. `Chataigne.jucer` and `Builds/Raspberry64/Makefile` are updated to link `-lgpiod` instead of building the pigpio sources.

PR test plan status: author built and ran `Builds/Raspberry64` Release on *a* Raspberry Pi and confirmed it links and no longer throws the init error. Two boxes unchecked: a check on Pi 5 hardware specifically, and confirmation of the `apt` package name for the `libgpiod` dev headers to document in the README.

## Key finding: libgpiod API version

The shim is written against the **libgpiod v2 API** (`gpiod_chip_open`, `gpiod_line_settings_new`, `gpiod_line_config_new`, `gpiod_chip_request_lines`, `gpiod_line_request_set_value`). libgpiod v1.x and v2.x are source-incompatible: v2 rewrote the whole API.

- Debian 13 trixie ships `libgpiod 2.2` : the shim compiles as-is.
- Debian 12 bookworm (and current Raspberry Pi OS, which is still bookworm-based) ships `libgpiod 1.6.3` : the shim will **not** compile there.

This is worth raising on the PR. As written it needs a trixie-class userland. The `Builds/Raspberry64` CI target and most users on Raspberry Pi OS are still on bookworm.

For testing the shim as written, a Pi 5 on Debian 13 trixie is the correct environment.

## Test box on hand

Andrea's Pi, reachable over SSH / Tailscale from the dev machine at any time.

| | |
|---|---|
| Model | Raspberry Pi 5 Model B Rev 1.1 |
| OS | Debian GNU/Linux 13 (trixie), aarch64 |
| CPU / RAM | 4 cores, 7.9 GiB (7.0 GiB free) |
| Disk | 37 GB free on `/` |
| Toolchain | g++ (Debian 14.2.0), GNU Make 4.4.1 |
| libgpiod runtime | `libgpiod3:arm64` 2.2.1, `gpiod` CLI 2.2.1 |
| libgpiod dev headers | not installed |
| gpiochips | `gpiochip0 [pinctrl-rp1]` (54 lines, the RP1), plus 4 brcmstb chips |
| webkit2gtk | no `-4.0` or `-4.1` dev package installed |
| gpio group | user is a member |
| Chataigne repo | not cloned on the Pi |

The shim's `openMainChip()` scans for a chip whose label contains `pinctrl` and falls back to `/dev/gpiochip0`. On this board `gpiochip0` is `pinctrl-rp1`, so both paths land on the right chip.

## Missing dependencies

```bash
sudo apt install -y libgpiod-dev libwebkit2gtk-4.1-dev
```

- `libgpiod-dev` : the `<gpiod.h>` header and the `.so` symlink the shim links against. This is the package name to document in the PR (answers the PR's open question). `pkg-config --modversion libgpiod` should then report 2.2.x.
- `libwebkit2gtk-4.1-dev` : the 4.1 bump the PR makes. Only needed for a full Chataigne build, not for the harness below.

## Two ways to test

### Option 1 : standalone harness (recommended, fast)

Compiles just the shim's 5 functions into a small program and exercises them against real hardware. Directly answers #214 (does GPIO init plus read / write / PWM work on a Pi 5) without building Chataigne or the JUCE fork.

Steps on the Pi:

```bash
sudo apt install -y libgpiod-dev
mkdir -p ~/gpio214 && cd ~/gpio214

# grab the exact shim from the PR head commit
BASE="https://github.com/benkuper/Chataigne/raw/25130b4f4ff422890268ff1d3f4d5c16dc3120dd/Source%2FModule%2Fmodules%2Fgpio%2Fgpiod"
curl -sL "$BASE%2FChataigneGPIO.c" -o ChataigneGPIO.c
curl -sL "$BASE%2FChataigneGPIO.h" -o ChataigneGPIO.h

# save the harness below as gpio_harness.c, then:
gcc -O2 -Wall gpio_harness.c ChataigneGPIO.c \
    $(pkg-config --cflags --libs libgpiod) -lpthread -o gpio_harness

# init-only, no wiring needed (this alone proves the #214 fix):
./gpio_harness

# full read / write / PWM check: jumper BCM17 (header pin 11) to BCM27 (header pin 13),
# or pass any two free BCM pins you have wired together:
./gpio_harness 17 27
```

Wiring for the full check: a single jumper wire between the two BCM pins is enough (output pin drives input pin). An LED plus ~330 ohm resistor from the output pin to ground also works and lets you eyeball the PWM ramp, which is a more reliable PWM check than the harness's coarse userspace sampling.

Exit code 0 and a final `PASS` line means the shim works on this board. A failure at `gpioInitialise()` is the exact #214 symptom and means the fix regressed.

#### `gpio_harness.c`

```c
/*
  gpio_harness.c : Pi 5 hardware check for Chataigne PR #335 (issue #214)

  Links the PR's libgpiod shim (ChataigneGPIO.c/.h) into a standalone program
  and exercises the 5 functions GPIOModule.cpp uses:
  gpioInitialise / gpioWrite / gpioRead / gpioPWM / gpioTerminate.

  Usage:
    ./gpio_harness              init + terminate only (no wiring needed)
    ./gpio_harness <out> <in>   loopback: jumper BCM <out> to BCM <in>

  Build (same dir as ChataigneGPIO.c / ChataigneGPIO.h):
    gcc -O2 -Wall gpio_harness.c ChataigneGPIO.c \
        $(pkg-config --cflags --libs libgpiod) -lpthread -o gpio_harness
*/

#include "ChataigneGPIO.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void nap_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

int main(int argc, char** argv) {
    if (gpioInitialise() != 0) {
        fprintf(stderr, "FAIL: gpioInitialise() returned an error "
                        "(this is the exact #214 symptom)\n");
        return 1;
    }
    printf("OK: gpioInitialise() succeeded on this board\n");

    if (argc < 3) {
        gpioTerminate();
        printf("OK: gpioTerminate() clean\n");
        printf("No pins given, skipped read/write/PWM. "
               "Pass <out> <in> BCM numbers with a wire between them.\n");
        printf("PASS (init-only)\n");
        return 0;
    }

    unsigned out = (unsigned)atoi(argv[1]);
    unsigned in  = (unsigned)atoi(argv[2]);
    int fails = 0;

    /* static level test */
    for (int want = 0; want <= 1; want++) {
        if (gpioWrite(out, (unsigned)want) != 0) {
            fprintf(stderr, "FAIL: gpioWrite(%u, %d)\n", out, want);
            fails++;
            continue;
        }
        nap_ms(20);
        int got = gpioRead(in);
        printf("write %d -> read %d  %s\n",
               want, got, got == want ? "ok" : "MISMATCH");
        if (got != want) fails++;
    }

    /* software PWM test: 50%% duty, sample the input and estimate duty */
    if (gpioPWM(out, 128) != 0) {
        fprintf(stderr, "FAIL: gpioPWM(%u, 128)\n", out);
        fails++;
    } else {
        int highs = 0, N = 400;
        for (int i = 0; i < N; i++) {
            if (gpioRead(in) == 1) highs++;
            nap_ms(2);
        }
        double duty = 100.0 * highs / N;
        printf("PWM 128/255 -> measured duty ~%.0f%% (expect roughly 30-70%%)\n",
               duty);
        if (duty < 20.0 || duty > 80.0) fails++;
        gpioPWM(out, 0);
    }

    gpioTerminate();
    printf("OK: gpioTerminate() clean\n");
    printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
```

### Option 2 : full Chataigne build on the Pi

Exercises the real integration in `GPIOModule.cpp`, not just the shim.

```bash
sudo apt install -y libgpiod-dev libwebkit2gtk-4.1-dev
# plus the rest of Chataigne's Linux build deps (see install_linux_deps.sh in the repo)

git clone --recursive https://github.com/dionzand/Chataigne.git -b gpio-libgpiod-migration
# also clone benkuper/JUCE branch develop-local and build the Projucer,
# then regenerate the Makefile or use the checked-in Builds/Raspberry64
cd Chataigne/Builds/Raspberry64
make CONFIG=Release -j4
./build/Chataigne -headless   # or run the GUI and add a GPIO module
```

Heavier: JUCE build is roughly a GB and takes a while on the Pi. Only worth it if the harness passes and we want end-to-end confirmation before approving the PR.

## What a result tells us for the PR

- Harness `PASS` on the Pi 5 : the libgpiod v2 shim works on RP1 hardware. Comment on the PR with the trixie caveat (needs libgpiod 2.x, so bookworm / current Raspberry Pi OS won't build it) and the `libgpiod-dev` package name, and suggest the README / build-deps note say trixie-or-newer.
- Harness fails at init : the shim regressed #214, block the PR.
- Harness read / write mismatch or PWM out of range : logic bug in the shim, request changes with the harness output.
