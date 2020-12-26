#!/bin/bash
#
# Collect_spectrum.sh
# Colect and process a braodband spectrum from an Airspy.
# Terry Bullett
# 02 Sep 2020
# Little Thompson Observatory
# "To explore His universe with Reverence"
#
#  Requires: airspy_rx, ra_spectrum, prefix.sh, update_waterfall.sh
#  Ted Clines Antpos
#
# Intended to be called from cron about every minute
#
#airspy_rx v1.0.5 23 April 2016
#Usage:
#-r <filename>: Receive data into file
#-w Receive data into file with WAV header and automatic name
# This is for SDR# compatibility and may not work with other software
# [-s serial_number_64bits]: Open device with specified 64bits serial number
# [-p packing]: Set packing for samples, 
#  1=enabled(12bits packed), 0=disabled(default 16bits not packed)
#  [-f frequency_MHz]: Set frequency in MHz between [24, 1900] (default 900MHz)
#  [-a sample_rate]: Set sample rate
#  [-t sample_type]: Set sample type, 
#   0=FLOAT32_IQ, 1=FLOAT32_REAL, 2=INT16_IQ(default), 3=INT16_REAL, 4=U16_REAL, 5=RAW
#   [-b biast]: Set Bias Tee, 1=enabled, 0=disabled(default)
#   [-v vga_gain]: Set VGA/IF gain, 0-15 (default 5)
#   [-m mixer_gain]: Set Mixer gain, 0-15 (default 5)
#   [-l lna_gain]: Set LNA gain, 0-14 (default 1)
#   [-g linearity_gain]: Set linearity simplified gain, 0-21
#   [-h sensivity_gain]: Set sensitivity simplified gain, 0-21
#   [-n num_samples]: Number of samples to transfer (default is unlimited)
# [-d]: Verbose mode

function cleanup {
   echo "$REALTIME - $APPNAME - WARN - Process terminating, removing $STATUSFILE"  >> $LOGFILE
   rm -f $STATUSFILE
   exit 0 
}
trap cleanup HUP INT QUIT TERM
HOME='/home/analysis/'
APPNAME='collect_spectrum'
STATUSFILE="$HOME/run/airspy_busy"
CALFILE="$HOME/run/LTO-CAL-2020-12-13-19-20-40.fft"
ANTFILE="$HOME/run/Lto16ftElLog200814b.txt"
TFMT='%F %T %Z'
REALTIME=$(date  -u +"$TFMT" )
YYYY=$(date -u '+%Y')
DDD=$(date -u '+%j')
LOGFILE="$HOME/log/$APPNAME-$YYYY-$DDD.log"
ERRLOG="$HOME/log/$APPNAME-$YYYY-$DDD.errlog"
SPYLOG="$HOME/log/$APPNAME-$YYYY-$DDD-collect.log"
LATEST='/data0/latest'
IQDIR='/cache/IQ'
ARCHIVE='raid1/IQ'
COLLECT='/run/airspy'
PREFIX="$(prefix.sh)"
# Cleans up on exit command
# Don't want to leave a stale airspy lock file
# Collection settings
declare -i SR NSAMP SAVERAW GI GM GL
# todo:  make this an external function
FRQ='1420.405751'
SR=$((10000000))
NSAMP=$((500000000))
NSAMP=$((593000000))
SAVERAW=$((0))
RAWDIR='/cache/IQ'
GAIN='7.77'

# IF Gain,  Mixer gain, LNA Gain
GI=$((8))
GM=$((8))
GL=$((11)) 
TFMT2='%Y%j%H%M%S'
PID="$$"
echo "$REALTIME - $APPNAME - INFO - Start run PID= $PID"  >> $LOGFILE
REALTIME=$(date -u +"$TFMT")  # for the logs
# Compatability with previous file names.  
SLEEP='0.1'
DTS=$(date -u +"$TFMT2")
mkdir -p "$RAWDIR"  2>> "$ERRLOG"
ARGSPY="-f $FRQ -a $SR -b 0 -t 2 -d -n $NSAMP -v $GI -m $GM -l $GL -w"
echo $REALTIME >> "$SPYLOG"
echo $ARGSPY >> "$SPYLOG"
# Date and time formats the same SDR# 
DATE=$(date '+%F'| tr '-' '_')
TIME=$(date '+%T'| tr ':' '-')
EPOCH=$(date -u '+%s' )
COLLECTDIR="$COLLECT/$DATE"
PROCESSDIR="$IQDIR/$DATE"
mkdir -p $COLLECTDIR $PROCESSDIR
while [ -e "STATUSFILE" ] ; do 
   REALTIME=$(date -u +"$TFMT" )   
   echo "$REALTIME - $APPNAME - WARN - Airspy is busy, sleeping $SLEEP"  >> $LOGFILE
done
cd "$COLLECTDIR"
# Start collect 
echo $$ > "$STATUSFILE"
airspy_rx $ARGSPY >> "$SPYLOG"
EXS="$?"
rm -f "$STATUSFILE"
find "$COLLECTDIR/" -type f -exec mv {} "$PROCESSDIR" \; 
RAWDIR="$RAWDIR/$DATE"
LTODIR="/data0/LTO/$DATE"
ARCHIVEDIR="/data0/Archive/$DATE"
# echo $RAWDIR $ARCHIVEDIR
mkdir -p $RAWDIR $ARCHIVEDIR
WAVFILE=$(grep 'Receive wav file:' $SPYLOG | tail -1 | awk '{print $4}' )
BFN=$(basename "$WAVFILE" '.wav')
LTOFILE="$PREFIX"-"$DATE"-"$TIME"
QRM="$LTOFILE"
FFTFILE="$LTOFILE"'.fft'
REALTIME=$(date  -u +"$TFMT" )
echo "$REALTIME - ra-spectrum - INFO -  Collect status $EXS collected $WAVFILE  "  >> $LOGFILE
# Call Teds code
AZ=$(Antpos "$ANTFILE" "$DATE" "$TIME" | awk '{print $1}')
EL=$(Antpos "$ANTFILE" "$DATE" "$TIME" | awk '{print $2}')
#db ARGS="-p 1.85  -w -b -Q -LO -PH -PS -PV -r 1.06 -s 5 -R 1.01 -ss 2.05 -I $QRM  -az $AZ -el $EL -d $DATE -t $TIME -e $EPOCH -cf $FRQ -o $COLELCTDIR/$LTOFILE -CAL $CALFILE "
ARGS="-p $GAIN  -w -b -LO -PH -PS -PV -r 1.06 -s 10 -R 1.02 -az $AZ -el $EL -d $DATE -t $TIME -e $EPOCH -cf $FRQ -I $QRM -o $PROCESSDIR/$LTOFILE -CAL $CALFILE"  
REALTIME=$(date  -u +"$TFMT" )
cd $PROCESSDIR
echo "$REALTIME - ra-spectrum - INFO -  $ARGS "  >> $LOGFILE
nice -n 5 ra-spectrum-v1.1 $ARGS $WAVFILE > $FFTFILE
echo 'FFTFILE= ' $FFTFILE 
REALTIME=$(date  -u +"$TFMT" )
# Check for empty fft file, bad run. 
if [ -s $FFTFILE ] ; then
   FFTPLOT=$(plot_fft.sh  $FFTFILE | awk '{print $8}') 
#  Later version FFTPLOT=$(plot_fft.sh -t "$FRQ" $FFTFILE | awk '{print $8}')    
   AVETSKY=$(grep Spectrum%AveTsky $FFTFILE | awk '{print $4}')
   VARTSKY=$(grep Spectrum%VarTsky $FFTFILE | awk '{print $4}')
   if (( $SAVERAW )); then
      mkdir -p "$ARCHIVE/$DATE"
      mv "$PROCESSIDR/$WAVFILE" "$ARCHIVE/$DATE/"
   else
      rm -f "$PROCESSDIR/$WAVFILE"
   fi
   mkdir -p "$LTODIR/" "$LATEST/fft"
   mv "$LTOFILE".lto "$LTODIR/"
   cp -a "$FFTPLOT" "$LATEST/latest_fft.png"
   mv "$FFTPLOT" "$ARCHIVEDIR/"
   cp -a "$FFTFILE" "$LATEST/fft/"
   mv "$FFTFILE" "$ARCHIVEDIR/" 
else
   REALTIME=$(date -u +"$TFMT" )   
   echo "$REALTIME - $APPNAME - ERROR - Empty $FFTFILE"  >> $LOGFILE
   rm -f "$FFTFILE"
fi 
exit 0 
