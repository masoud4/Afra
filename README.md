# Afra - Screen Recorder

Afra is a simple screen recorder app that uses Xlib and ffmpeg to capture your desktop screen. It is intended for personal use and is currently only capable of recording video without audio.
Requirements

To use Afra, you need to have the following installed on your system:

    Xlib
    ffmpeg

# Installation

	git clone https://github.com/masoud4/Afra.git

	cd Afra

	make install



## Usage


	./Afra filename codeec


For example, to record your desktop screen as myvideo.mp4, you can use the following command:

	./Afra  myvideo.mp4 mpeg4

## License

Afra is licensed under the MIT License. See LICENSE file for more information.
