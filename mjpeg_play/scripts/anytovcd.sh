#!/bin/sh
#
#  anytovcd.sh
#
#  Copyright (C) 2004 Nicolas Boos <nicolaxx@free.fr>
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

AUD_TRACK="1"
FILTERING="none"
FILTER_TYPE="median"
VCD_TYPE="dvd"
QUALITY="best"
VERSION="2"

BFR="bfr"
FFMPEG="ffmpeg"
MPEG2DEC="mpeg2dec"
MPEG2ENC="mpeg2enc"
MPLEX="mplex"
PGMTOY4M="pgmtoy4m"
SOX="sox"
TRANSCODE="transcode"
Y4MSCALER="y4mscaler"
Y4MSPATIALFILTER="y4mspatialfilter"
YUVDENOISE="yuvdenoise"
YUVMEDIANFILTER="yuvmedianfilter"

probe_aud_fmt ()
{
    # $1 = audio input
    # $2 = audio track
    echo "`${FFMPEG} -map 0:${2} -i "$1" 2>&1 | \
    awk '/Audio:/ {print $4}' | sed s/,// | head -${2} | tail -1`"
}

probe_aud_freq ()
{
    # $1 = audio input
    # $2 = audio track
    echo "`${FFMPEG} -map 0:${2} -i "$1" 2>&1 | \
    awk '/Audio:/ {print $5}' | sed s/,// | head -${2} | tail -1`"
}

probe_vid_fps ()
{
    ${TRANSCODE} -i "$1" -c 0-1 -y yuv4mpeg -o /tmp/tmp.y4m >/dev/null 2>&1
    echo "`head -1 /tmp/tmp.y4m | awk '{print $4}' | cut -c 2-`"
    rm -f /tmp/tmp.y4m
}

probe_vid_ilace ()
{
    ${FFMPEG} -i "$1" -f yuv4mpegpipe -t 1 -y /tmp/tmp.y4m >/dev/null 2>&1
    echo "`head -1 /tmp/tmp.y4m | awk '{print $5}' | cut -c 2-`"
    rm -f /tmp/tmp.y4m
}

probe_vid_sar ()
{
    ${TRANSCODE} -i "$1" -c 0-1 -y yuv4mpeg -o /tmp/tmp.y4m >/dev/null 2>&1
    echo "`head -1 /tmp/tmp.y4m | awk '{print $6}' | cut -c 2-`"
    rm -f /tmp/tmp.y4m
}

probe_vid_fmt ()
{
    echo "`${FFMPEG} -i "$1" 2>&1 | \
    awk '/Video:/ {print $4}' | sed s/,// | head -1`"
}

show_help ()
{
    echo "----------------------------------"
    echo "-a    audio track number (1)      "
    echo "-b    blind mode (no video)       "
    echo "-d    don't scale video           "
    echo "-i    input file                  "
    echo "-f    filtering method (*none*)   "
    echo "      avail.: light, medium, heavy"
    echo "-m    mute mode (no audio)        "
    echo "-p    output type (default = dvd) "
    echo "      avail. : cvd, dvd, dvd_wide,"
    echo "               svcd and vcd       "
    echo "-q    quality (default = best)    "
    echo "      avail. : best, good, fair   "
    echo "-t    filter type to use with     "
    echo "      the chosen filter method    "
    echo "      (default = median)          "
    echo "      avail. : mean (yuvmedianfilter -f) "
    echo "               median (yuvmedianfilter)  "
    echo "               spatial (y4mspatialfilter)"
    echo "               temporal (yuvdenoise)     "
    echo "-v    script version              "
}

test_bin ()
{
    if ! which ${1} >/dev/null; then

        echo "Error : ${1} not present."
        exit 2

    fi
}

for BIN in ${FFMPEG} ${MPEG2ENC} ${MPLEX} ${PGMTOY4M} ${TRANSCODE}; do

    test_bin ${BIN}

done

while getopts a:bdf:i:mp:q:t:v OPT
do
    case ${OPT} in
    a)    AUD_TRACK="${OPTARG}";;
    b)    BLIND_MODE="1";;
    d)    SCALING="none";;
    i)    VIDEO_SRC="${OPTARG}"; AUDIO_SRC="${VIDEO_SRC}";;
    f)    FILTERING="${OPTARG}";;
    m)    MUTE_MODE="1";;
    p)    VCD_TYPE="${OPTARG}";;
    q)    QUALITY="${OPTARG}";;
    t)    FILTER_TYPE="${OPTARG}";;
    v)    echo "${VERSION}"; exit 0;;
    \?)   show_help; exit 0;;
    esac
done

if test "${VIDEO_SRC}" == "" || ! test -r "${VIDEO_SRC}"; then

    echo "Error : input file not specified or not present."
    show_help
    exit 2

fi

AUD_FMT_SRC="`probe_aud_fmt "${AUDIO_SRC}" ${AUD_TRACK}`"
AUD_FREQ_SRC="`probe_aud_freq "${AUDIO_SRC}" ${AUD_TRACK}`"
VID_FPS_SRC="`probe_vid_fps "${VIDEO_SRC}"`"
VID_ILACE_SRC="`probe_vid_ilace "${VIDEO_SRC}"`"
VID_SAR_SRC="`probe_vid_sar "${VIDEO_SRC}"`"
VID_FMT_SRC="`probe_vid_fmt "${VIDEO_SRC}"`"

if test "${VID_SAR_SRC}" == "0:0"; then
    VID_SAR_SRC="1:1"
fi

MPEG2ENC="${MPEG2ENC} -v 0 -M 0 -R 2 -P -s -2 1 -E -5"
PGMTOY4M="${PGMTOY4M} -r ${VID_FPS_SRC} -i ${VID_ILACE_SRC} -a ${VID_SAR_SRC}"
Y4MSCALER="${Y4MSCALER} -v 0"

# filtering method(s)
if test "${FILTERING}" == "none"; then

    YUVMEDIANFILTER=""
    YUVDENOISE=""
    Y4MSPATIALFILTER=""
    MEAN_FILTER=""

elif test "${FILTERING}" == "light"; then

    YUVMEDIANFILTER="${YUVMEDIANFILTER} -t 0"
    YUVDENOISE="${YUVDENOISE} -l 1 -t 4 -S 0"
    Y4MSPATIALFILTER="${Y4MSPATIALFILTER} -x 0 -y 0"
    MEAN_FILTER="${YUVMEDIANFILTER} -f -r 1 -R 1"

elif test "${FILTERING}" == "medium"; then

    YUVMEDIANFILTER="${YUVMEDIANFILTER} -t 1 -T 1"
    YUVDENOISE="${YUVDENOISE} -l 2 -t 6 -S 0"
    Y4MSPATIALFILTER="${Y4MSPATIALFILTER} -X 4,0.64 -Y 4,0.8"
    MEAN_FILTER="${YUVMEDIANFILTER} -f -r 1 -R 1 -w 2.667"

elif test "${FILTERING}" == "heavy"; then

    YUVMEDIANFILTER="${YUVMEDIANFILTER}"
    YUVDENOISE="${YUVDENOISE} -l 3 -t 8 -S 0"
    Y4MSPATIALFILTER="${Y4MSPATIALFILTER} -X 4,0.5 -Y 4,0.5 -x 2,0.5 -y 2,0.5"
    MEAN_FILTER="${YUVMEDIANFILTER} -f"

else

    echo "Error : the specified filtering method is inexistant."
    show_help
    exit 2

fi

# profile(s)
if test "${VCD_TYPE}" == "cvd"; then

    VIDEO_OUT="out.m2v"
    AUDIO_OUT="out.ac3"
    MPEG_OUT="out.mpg"

    MPEG2ENC="${MPEG2ENC} -f 8"
    Y4MSCALER="${Y4MSCALER} -O preset=cvd -S option=cubicCR"
    MPLEX="${MPLEX} -f 8"

    AUD_FMT_OUT="ac3"
    AUD_BITRATE_OUT="192"
    AUD_FREQ_OUT="48000"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "dvd"; then

    VIDEO_OUT="out.m2v"
    AUDIO_OUT="out.ac3"
    MPEG_OUT="out.mpg"

    MPEG2ENC="${MPEG2ENC} -f 8 -b 8000 -D 10"
    Y4MSCALER="${Y4MSCALER} -O preset=dvd -S option=cubicCR"
    MPLEX="${MPLEX} -f 8"

    AUD_FMT_OUT="ac3"
    AUD_BITRATE_OUT="192"
    AUD_FREQ_OUT="48000"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "dvd_wide"; then

    VIDEO_OUT="out.m2v"
    AUDIO_OUT="out.ac3"
    MPEG_OUT="out.mpg"

    MPEG2ENC="${MPEG2ENC} -f 8 -b 8000 -D 10"
    Y4MSCALER="${Y4MSCALER} -O preset=dvd_wide -S option=cubicCR"
    MPLEX="${MPLEX} -f 8"

    AUD_FMT_OUT="ac3"
    AUD_BITRATE_OUT="192"
    AUD_FREQ_OUT="48000"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "svcd"; then

    VIDEO_OUT="out.m2v"
    AUDIO_OUT="out.mp2"
    MPEG_OUT="out-%d.mpg"

    MPEG2ENC="${MPEG2ENC} -f 4 -B 256 -S 797"
    Y4MSCALER="${Y4MSCALER} -O preset=svcd -S option=cubicCR"
    MPLEX="${MPLEX} -f 4"

    AUD_FMT_OUT="mp2"
    AUD_BITRATE_OUT="224"
    AUD_FREQ_OUT="44100"
    AUD_CHANNELS_OUT="2"

elif test "${VCD_TYPE}" == "vcd"; then

    VIDEO_OUT="out.m1v"
    AUDIO_OUT="out.mp2"
    MPEG_OUT="out-%d.mpg"

    MPEG2ENC="${MPEG2ENC} -f 1 -B 256 -S 797"
    Y4MSCALER="${Y4MSCALER} -O preset=vcd -S option=cubic"
    MPLEX="${MPLEX} -f 1"

    AUD_FMT_OUT="mp2"
    AUD_BITRATE_OUT="224"
    AUD_FREQ_OUT="44100"
    AUD_CHANNELS_OUT="2"

    if test "${VID_ILACE_SRC}" == "b"; then
        Y4MSCALER="${Y4MSCALER} -I ilace=bottom_only"
    elif test "${VID_ILACE_SRC}" == "t"; then
        Y4MSCALER="${Y4MSCALER} -I ilace=top_only"
    fi

else

    echo "Error : the specified output type/profile is inexistant."
    show_help
    exit 2

fi

# quality preset(s)
if test "${VCD_TYPE}" == "vcd"; then

    # when using -q with mpeg2enc, variable bitrate is selected.
    # it's not ok for vcds...
    MPEG2ENC="${MPEG2ENC}"

elif test "${QUALITY}" == "best"; then

    MPEG2ENC="${MPEG2ENC} -q 4 -K kvcd"

elif test "${QUALITY}" == "good"; then

    MPEG2ENC="${MPEG2ENC} -q 5 -K tmpgenc"
    
elif test "${QUALITY}" == "fair"; then

    MPEG2ENC="${MPEG2ENC} -q 6 -K kvcd -N 0.4"

else

    echo "Error : the specified quality preset is inexistant."
    show_help
    exit 2

fi

# pipe buffer
if which ${BFR} >/dev/null; then

    PIPE_BUFFER="${BFR} -b 16m |"

fi

# video decoder
if test "${VID_FMT_SRC}" == "mpeg2video" || test "${VID_FMT_SRC}" == "mpeg1video" && which ${MPEG2DEC} >/dev/null; then

    DECODER="${MPEG2DEC} -s -o pgmpipe \"${VIDEO_SRC}\" | ${PGMTOY4M} |"

else

    DECODER="${FFMPEG} -i \"${VIDEO_SRC}\" -f imagepipe -img pgmyuv -y /dev/stdout | ${PGMTOY4M} |"

fi

# video filter
if test "${FILTERING}" == "none"; then

    FILTER=""

elif test "${FILTER_TYPE}" == "mean"; then
    
    test_bin ${YUVMEDIANFILTER}
    FILTER="${MEAN_FILTER} |"

elif test "${FILTER_TYPE}" == "median"; then

    test_bin ${YUVMEDIANFILTER}
    FILTER="${YUVMEDIANFILTER} |"

elif test "${FILTER_TYPE}" == "spatial"; then

    test_bin ${Y4MSPATIALFILTER}
    FILTER="${Y4MSPATIALFILTER} |"

elif test "${FILTER_TYPE}" == "temporal"; then

    test_bin ${YUVDENOISE}
    FILTER="${YUVDENOISE} |"

else

    echo "Error : the specified filter tool is not used by this script."
    show_help
    exit 2

fi

# video scaler
if test "${SCALING}" == "none"; then

    SCALER=""

else

    test_bin ${Y4MSCALER}
    SCALER="${Y4MSCALER} |"

fi

# video encoder
ENCODER="${MPEG2ENC} -o ${VIDEO_OUT}"

# audio resampler
if test "${AUD_FREQ_SRC}" == "${AUD_FREQ_OUT}"; then

    AUDIO_RESAMPLER=""

else

    test_bin ${SOX}
    AUDIO_RESAMPLER="${SOX} -t au /dev/stdin -r ${AUD_FREQ_OUT} -t au /dev/stdout polyphase |"

fi

# audio decoder/encoder
if test "${AUD_FMT_SRC}" == "${AUD_FMT_OUT}"; then

    AUDIO_DECODER="${FFMPEG} -map 0:${AUD_TRACK} -i \"${AUDIO_SRC}\""
    AUDIO_ENCODER="-acodec copy -y ${AUDIO_OUT}"

elif test "${AUD_FREQ_SRC}" == "${AUD_FREQ_OUT}"; then

    AUDIO_DECODER="${FFMPEG} -map 0:${AUD_TRACK} -i \"${AUDIO_SRC}\""
    AUDIO_ENCODER="-ab ${AUD_BITRATE_OUT} -ac ${AUD_CHANNELS_OUT} -y ${AUDIO_OUT}"

else

    AUDIO_DECODER="${FFMPEG} -v 0 -map 0:${AUD_TRACK} -i \"${AUDIO_SRC}\" -f au -y /dev/stdout |"
    AUDIO_ENCODER="${FFMPEG} -f au -i /dev/stdin -ab ${AUD_BITRATE_OUT} -ac ${AUD_CHANNELS_OUT} -y ${AUDIO_OUT}"

fi

# video (de)coding
if ! test "${BLIND_MODE}" == "1"; then

    eval "${DECODER} ${FILTER} ${SCALER} ${PIPE_BUFFER} ${ENCODER}"

fi

# audio (de)coding
if ! test "${MUTE_MODE}" == "1"; then

    eval "${AUDIO_DECODER} ${AUDIO_RESAMPLER} ${AUDIO_ENCODER}"

fi

# multiplexing
if ! test "${BLIND_MODE}" == "1" && ! test "${MUTE_MODE}" == "1"; then

    ${MPLEX} -o ${MPEG_OUT} ${VIDEO_OUT} ${AUDIO_OUT}

fi

exit 0
