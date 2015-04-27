# Remember to install debuginfo for httpd using the 'debuginfo-install httpd' command

all:
	apxs -i -a -c mod_exp_receive.c
	cp mod_exp_receive.conf /etc/httpd/conf.d

