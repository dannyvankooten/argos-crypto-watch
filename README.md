Lightweight C program for monitoring cryptocoin price changes in [Argos](https://github.com/p-e-w/argos).

### Usage

Invoke the program using a list of the coins to show as command-line arguments. Names should match the URL part of CoinMarketCap.com.

```sh
$ crypto_watch bitcoin ethereum polkadot chainlink

BTC $49100.30           ETH $1568.24            DOT $36.54              LINK $29.68
---
BTC             $  49100.30 (+5%)               $ 53378M (+5%)
ETH             $   1568.24 (+7%)               $ 23833M (-2%)
DOT             $     36.54 (+6%)               $  3536M (-14%)
LINK            $     29.68 (+16%)              $  2940M (+77%)
---
Last updated at 10:44 AM | refresh=true 
```


### Building from source

```sh
git clone git@github.com:dannyvankooten/argos-crypto-watch.git --recurse-submodules
make 
make install
```

