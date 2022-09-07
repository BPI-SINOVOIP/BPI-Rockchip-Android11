#!/sbin/busybox sh

result_file=/data/ether_result.txt
interface_up=true
jmax=3

echo "touch $result_file"
busybox touch $result_file

j=0

echo "Get ping results"
while [ $j -lt $jmax ]; 
do
    if busybox ifconfig eth0; then
        if [ $interface_up = "true" ]; then
            busybox ifconfig eth0 up
        fi

		#use ping type
		if [ "$3" = "1" ]; then
	    	busybox ifconfig eth0 $1

		echo "sleep 2s"
	    	busybox sleep 2
	    	
	    	#busybox ping -c 5 -W 5 $2 | busybox grep seq > $result_file
	    	busybox ping -c 5 -W 5 $2 | busybox grep "packet loss" > $result_file
	        echo "success local $1 ping $2"
	        busybox cat $result_file
	        exit 1
	        
	    #use view local address
	    else
	    	echo "sleep 2s"
	    	busybox sleep 2

	    	busybox ifconfig eth0 | busybox grep inet > $result_file
	    	
			#Check result
			if [ -s $result_file ]; then
				echo "the local ip address from dhcp assign"
	    		busybox cat $result_file
	    		exit 1
		    	else
		    	echo "the local ip address is Null"
		    fi
	    	
	    fi
	    
        
    fi

	#Clear ethernet address
	if [ "$3" = "1" ]; then
	    echo "Clear eth0 addr"
	    busybox ifconfig eth0 0.0.0.0
	fi
	
    busybox sleep 2
    
    j=$((j+1))
done

echo "lan test failed"
exit 0

