intall:
	clang  Afra.c `pkg-config --cflags --libs libavcodec libavformat libavutil x11` -o Afra
