# A script that converts the data from input to 
# Mpeg Streams
#


if [ $# -lt 2 ] 
then 
  echo  1nd option must specify basename of the images
  echo  2rd option must specify directory where the JPG\'s will be 
  echo  3st option must specify the file with the audio and video stream 
  echo  4th option must specify output file name
  echo  If option is "n" nothing is done for this step 
  echo  The last option has not to be suplied if not used
  echo  
  echo  Example: acon.sh n n stream.movtar ownvideo.mpg
  echo  Would do: create an MPEG audio and video stream and 
  echo  put the streams together to ownvideo.mpg
  echo  Example: acon.sh exile pics stream.movtar ownvideo.mpg
  echo  Would do: If directory bmp exists convert all File from there to jpg format 
  echo  in directory pics. Then the the jpg\'s and the file sound.wav will be put 
  echo  together stream.movtar. Then the stream will be converted to ownvideo.mpg
  echo 
  echo  The source files must be deleted manually, or uncomment some line in the script 
  echo  For more information read the script 

exit 0
fi 
# Ausstieg

echo given Options $1 $2 $3 $4


# Converting the Images to an mpeg1 stream
if [ -n "$1" -a -n "$2" -a -n "$3" -a "$1" != "n" -a "$2" != "n" ] 
then
   if [ ! -d "$2" ]
   then
     mkdir $2
   fi

   if [ -d bmp ]
   then 
     cd bmp
     for A in *
       do
         convert $A -quality 95 ../jpg/$A.jpg
       done
     cd ..
   fi

   cd $2

   for A in *.jpg 
     do
       buz_jpegfilter < $A > `basename $A .bmp.jpg`.con.jpeg 
     done

   cd ..

   movtar_unify -j $2/$1%04d.con.jpeg -w sound.wav -o $3 

# Uncomment this lines if the files shoud be deleted after creating the movtar
#  rm -R $2 
#  rm -R sound.wav  
#  if [ -d bmp ]
#    then 
#      rm -R bmp
#    fi 

   echo You have to remove the Files in bmp/ and $2/ manually \n

fi 

# Audio conversation to sound.mpg  -b means Bitrate in kBit/s
# Video conversation to video.mpg  -b means Bitrate in kBit/s
if [ -n "$3" -a "$3" != "n" ]
then 
   aenc -b 224 -o sound.mpg $3
   mpeg2enc -b 2000 -o video.mpg $3 
fi

# Last step: putting the streams together
if [ -n "$4" ]
then 
   mplex sound.mpg video.mpg $4 
fi
