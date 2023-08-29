# https://stackoverflow.com/a/1521783/21294350； new version `sed` needs `p` if not in the shell script.
for i in $(ls tests/*.out);do perl -pi -e 's/\n/\r\n/' $i;done
# the following also works. https://unix.stackexchange.com/a/138475/568529
# echo -ne "1\n2" | unix2dos | xxd
