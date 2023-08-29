for i in $(ls tests/*.out);do perl -pi -e 's/\n/\r\n/' $i;done
