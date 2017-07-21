#!/bin/sh
# pktgen.conf -- Sample configuration for send on two devices on a UP system
#modprobe pktgen
function pgset() {
	local result
        echo $1 > $PGDEV   
        result=`cat $PGDEV | fgrep "Result: OK:"`
	#local content
	#content=`cat $PGDEV`	
	#echo $content      
	if [ "$result" = "" ]; then
             cat $PGDEV | fgrep Result:
        fi
}
    
function pg() {
    echo inject > $PGDEV
    cat $PGDEV
}
    
    # On UP systems only one thread exists -- so just add devices
    # We use eth1, eth2
    
    echo "Adding devices to run".
    
    PGDEV=/proc/net/pktgen/kpktgend_0

    BASIC_DELAY=1000000000 #1pps
	BASIC_SPEED=1    
	SEND_TOTAL_TIME=10
	BLOCK_TIME=5
	DATA=a.txt
    	
    pgset "rem_device_all"
    pgset "add_device h4s1-eth0"
    pgset "max_before_softirq 10000"
    
    # Configure the individual devices
    echo "Configuring devices"
    
    PGDEV=/proc/net/pktgen/h4s1-eth0
    
    pgset "clone_skb 0"
    pgset "pkt_size 60"
    pgset "src_mac 00:00:00:00:00:04"
    pgset "src_min 10.0.0.4"
    pgset "src_max 10.0.0.4"
    pgset "dst_min 10.0.0.3"
    pgset "dst_min 10.0.0.3"
pgset "udp_src_min 60"    
pgset "udp_src_max 6555"
pgset "udp_dst_min 60"    
pgset "udp_dst_max 6555"
    pgset "dst_mac 00:00:00:00:00:03"

    
    # Time to run
    echo "">a.txt
   
    SPEED=2
    echo "Running... ctrl^C to stop"
    for (( i=1; i<10000; i++ ));do
	PGDEV=/proc/net/pktgen/h4s1-eth0
	
	SPEED=`expr 1000`
#+ $SPEED`
	DELAY=`expr ${BASIC_DELAY} / $SPEED`
	PPS=`expr ${SPEED} \* ${BASIC_SPEED}`
	if [ $PPS -gt 50000000 ];then
	    #echo $PPS
	    PPS=50000000
	fi
	PKT_COUNT=`expr $SEND_TOTAL_TIME \* $PPS`
	echo "${PPS}/pps,send ${PKT_COUNT}pkt"
	
	pgset "count $PKT_COUNT"
	pgset "delay $DELAY"	
	
	PGDEV=/proc/net/pktgen/pgctrl   
	#mark start
	echo ${PPS} >> $DATA
	pgset "start"
	
	CONTENT=`cat /proc/net/pktgen/h4s1-eth0`
	echo $CONTENT

	#echo 'sleep ${BLOCK_TIME}s'
	#echo 'sleep' >> $DATA
	#sleep ${BLOCK_TIME}s
	#echo 'wake up'
    done    
    echo "Done"
