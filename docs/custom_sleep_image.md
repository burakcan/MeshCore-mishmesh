# Custom Sleep Image

On supported e-ink devices, select **Settings → Display → Sleep screen → Custom image** to show a portrait image while the screen sleeps. The panel turns to portrait for the image and restores the UI orientation on wake. The image refreshes once per sleep.

## Replace the Image

The source image is [`mishmesh/assets/custom_sleep_image.png`](../mishmesh/assets/custom_sleep_image.png). It must be exactly **122 × 250 pixels**. Replace it, then regenerate the firmware bitmap:

```sh
python3 mishmesh/tools/generate_sleep_image.py
```

The generator requires Pillow (`python3 -m pip install Pillow`). It updates [`mishmesh/core/custom_sleep_image.h`](../mishmesh/core/custom_sleep_image.h), a 4,000-byte monochrome bitmap compiled into the firmware. Normal firmware builds use the generated header and do not require Pillow.

The conversion composites transparency onto white and thresholds grayscale at 128. Black pixels are drawn on the e-ink panel; white pixels remain the background.
