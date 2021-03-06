# Grabber

[![Build Status](https://travis-ci.org/Bionus/imgbrd-grabber.svg)](https://travis-ci.org/Bionus/imgbrd-grabber)
[![Coverage Status](https://coveralls.io/repos/Bionus/imgbrd-grabber/badge.svg)](https://coveralls.io/r/Bionus/imgbrd-grabber)
[![GitHub issues](https://img.shields.io/github/issues/Bionus/imgbrd-grabber.svg)](https://github.com/Bionus/imgbrd-grabber/issues)

## About
Imgbrd-Grabber is an imageboard/booru downloader with graphical user interface. It can download thousands of images from multiple boorus automatically.
Its main advantage is its very powerful naming features: just set your filename and folder using all the tokens available and conditionals, and he will generate an image-dependant one.

## Contact
If you have any questions about the program, found a bug and don't want to use the github issue tracker, or anything, you can contact me by mail in French or in English at [bio.nus@hotmail.fr](mailto:bio.nus@hotmail.fr).

## Main features
* Browse images from the internet
* Download huge amounts of images
* Download single images using their md5 or id
* Rename downloaded images using a formatting string, for example "%artist%/%copyright%/%character%/%md5%.%ext%" (someway like how the Firefox extension [Danbooru Downloader](https://addons.mozilla.org/fr/firefox/addon/danbooru-downloader/) does), or directly using Javascript code. See {{Filename}} for details.
* Add and remove imageboards very easily
* Multiple tabs
* Display multiple imageboards at the same time in a single tab
* Merge results from these imageboards (i.e. remove duplicates in results)
* Favorite & "View it later" tags
* Auto-completion in the search field
* Post-filtering
* Autodownload
* Can import settings from Mozilla Firefox extension "danbooru downloader"
* Can add entries to a database for each image or tag while downloading
* Blacklist
* Fullscreen
* Proxy support
* ... and more!
You can also have a more complete list on the {{Changelog}}.

## Languages
* English
* French
* Russian (thanks to Николай Тихонов)

## Authors
* [Bionus](https://github.com/Bionus)

## Thanks
* YMI for all his suggestions and helping debbuging the program
* Николай Тихонов: for the Russian translation

## License
The program is licensed under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0).

## Compilation on Linux

```
./build.sh
```

Or if you want to run the commands yourself :

```
sudo add-apt-repository --yes ppa:ubuntu-sdk-team/ppa
sudo apt-get update -qq
sudo apt-get install -qq qt5-qmake qtbase5-dev qtdeclarative5-dev qtscript5-dev qtmultimedia5-dev libpulse-dev
sudo apt-get install qt5-default qttools5-dev-tools
qmake Grabber.pro
make
mv Grabber release/Grabber
touch release/settings.ini
```

If you want more details, see the [Compilation](https://github.com/Bionus/imgbrd-grabber/wiki/Compilation) wiki page.
