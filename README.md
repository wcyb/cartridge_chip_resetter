# Cartridge chip resetter

This repository contains electronic schematics and source code for resetters of chips placed on ink and toner cartridges. Resetters allow the same cartridge to be reused any number of times after it has been refilled with ink or toner respectively. This allows for maximum reduction in printing costs, as you only need to refill the cartridges and reset their chips when necessary, instead of buying a new cartridge every time.

Ready-made resetters of Chinese origin, for different cartridge models can be purchased on various sites, such as AliExpress or eBay. If you want to make such a resetter for your printer yourself, you can use this repository.

For more information on how the DRM protections used in printer cartridges work, and how to analyze them and create resetters yourself, see my research paper, available here: [research paper](https://czasopisma.uwm.edu.pl/index.php/ts/article/view/7634).

Currently, this repository contains documentation of resetters for printers and MFPs from two manufacturers - EPSON and RICOH.

These resetters should work with chips from cartridges used in the following printer/MFP models:

| EPSON     | RICOH         |
|-----------|---------------|
| B40W      | SG 2100N      |
| BX300F    | SG 3100 SNw   |
| BX310F    | SG 3110DN     |
| BX600FW   | SG 3110DNw    |
| BX610FW   | SG 3110 SFNw  |
| D78       | SG 3120B SFNw |
| D92       | SG 7100DN     |
| D120      | SP 100        |
| DX4000    | SP 100E       |
| DX4050    | SP 100SU      |
| DX4400    | SP 100SF      |
| DX4450    | SP 112        |
| DX5000    | SP 112SU      |
| DX5050    | SP 112SF      |
| DX6000    |               |
| DX6050    |               |
| DX7000    |               |
| DX7000F   |               |
| DX7400    |               |
| DX7450    |               |
| DX8400    |               |
| DX8450    |               |
| DX9000    |               |
| DX9400    |               |
| DX9400F   |               |
| DX9450    |               |
| DX945     |               |
| S20       |               |
| S21       |               |
| SX100     |               |
| SX105     |               |
| SX110     |               |
| SX115     |               |
| SX200     |               |
| SX210     |               |
| SX215     |               |
| SX218     |               |
| SX400     |               |
| SX405     |               |
| SX405WiFi |               |
| SX410     |               |
| SX415     |               |
| SX510W    |               |
| SX515W    |               |
| SX600FW   |               |
| SX610FW   |               |

Cartridge models:

| EPSON | Compatible resetter | RICOH   | Compatible resetter |
|-------|---------------------|---------|---------------------|
| T0711 | DX4050              | GC 41K  | SG2100N             |
| T0712 | DX4050              | GC 41C  | SG2100N             |
| T0713 | DX4050              | GC 41M  | SG2100N             |
| T0714 | DX4050              | GC 41Y  | SG2100N             |
|       |                     | GC 41KL | SG2100N             |
|       |                     | GC 41CL | SG2100N             |
|       |                     | GC 41ML | SG2100N             |
|       |                     | GC 41YL | SG2100N             |
|       |                     | _IC 41_ | SG2100N             |
|       |                     | 407166  | SP112               |

This gives a total of 13 cartridge models and 1 waste ink container model, used in 60 different printer and MFP models.

A data map for the chip used in each cartridge model is also included in the folders for each of the resetters. It is not guaranteed to be completely compatible with chip data from printer models other than the one on which it was based. However, if there are any differences, they should be minor and not affect the functioning of the resetter.

I don't plan to create any more resetters as I don't own any other printer models. If someday I have a new printer, then maybe the documentation for another resetter will appear here.

### Non-commercial use only.
