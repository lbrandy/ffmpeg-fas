cd `dirname $0`
FFMPEG_BASEDIR=./ffmpeg/

rm -rf lib
mkdir lib

gcc ffmpeg_fas.c seek_indices.c -Iffmpeg ffmpeg/libavformat/libavformat.a ffmpeg/libavcodec/libavcodec.a ffmpeg/libavutil/libavutil.a -O2 -shared -o lib/libffmpeg_fas.so
gcc -c ffmpeg_fas.c seek_indices.c -O2 -I$FFMPEG_BASEDIR
ar rc lib/libffmpeg_fas.a ffmpeg_fas.o seek_indices.o
