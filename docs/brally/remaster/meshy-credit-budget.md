# Meshy credit budget

*Recorded 2026-09-30.*

> RULE -- Meshy textures/images cost 10-20 credits EACH at most; never plan tiling/upscale schemes that multiply credits

A Meshy-generated texture (skymap, concept image, etc.) gets a budget of 10-20 credits TOTAL, at most. One 9-credit image (nano-banana-pro / gpt-image-2) plus at most one repair pass. Do resolution work (wrapping seams, upscaling, blending) locally in code, not with more Meshy calls.

**Why:** 2026-09-29, skymaps task: I test-spent ~45 credits and was planning ~250 credits per sky via i2i super-resolution tiling. the project lead: "don't burn the whole budget on this are you crazy!" Balance was ~2900.

**How to apply:** before any Meshy batch, state credits per asset and total; stay within 20/asset. No model-comparison test runs across several paid models. Related: [remaster-car-meshy-pipeline](remaster-car-meshy-pipeline.md).
