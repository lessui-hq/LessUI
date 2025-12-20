readmes:
ifneq ($(wildcard ../readmes/BASE-in.txt),)
	# formating readme
	cd ../readmes && fmt -w 40 -s ./BASE-in.txt > ./BASE-out.txt
	cd ../readmes && rm ./*-in.txt
else
	# skipped readme
endif