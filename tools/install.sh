#############################################################################
#############################################################################
#
# install.sh
#
#############################################################################
#############################################################################





#############################################################################
# copy_file
#############################################################################
copy_file()
	{
	[ -e "$1" ] || return
	echo "copying '$1' -> '$2'" &&
	chown root:root "$1" &&
	mkdir -p "${2%/*}" &&
	cp -a "$1" "$2" || exit 1
	}



#############################################################################
# remove_file
#############################################################################
remove_file()
	{
	[ -e "$1" ] || return
	echo "removing '$1'" &&
	rm -f "$1" || exit 1
	}



#############################################################################
# check_char_major
#############################################################################
check_char_major()
	{
	awk -v major="$MAJOR" '/^Character devices:$/ {
		s = 0;
		}

		{
		if (s == 1) next;
		}

	/^$/ {
		s = 1;
		}

	(s == 0) && ($1 == major) {
		q = "'\''";
		print >"/dev/stderr";
		printf("found the above line in %s/proc/devices%s, which\n", q, q) >"/dev/stderr";
		printf("indicates that char major %s is already in use!\n", major) >"/dev/stderr";
		exit(1);
		}' /proc/devices || exit 1
	}



#############################################################################
# modify_modules_conf
#############################################################################
modify_modules_conf()
	{
	if [ ! -e "$MODULES_CONF" ]; then
		echo "file '$MODULES_CONF' not found"
		return
	fi
	LINES="$(awk -v file="$MODULES_CONF" '{
		line++;
		}

	/^options '"$MODULE_NAME"' cw_major='"$MAJOR"'$/ ||
	/^alias char-major-'"$MAJOR $MODULE_NAME"'$/ {
		printf("%8d %s\n", line, $0);
		next;
		}

	/^[ \t]*(alias[ \t]+char-major-[0-9]+|options)[ \t]+'"$MODULE_NAME"'([ \t]+|$)/ ||
	/^[ \t]*alias[ \t]+char-major-'"$MAJOR"'/ {
		i++;
		printf("%8d %s\n", line, $0) >"/dev/stderr";
		next;
		}

	END {
		if (i == 0) exit (0);
		q = "'\''";
		printf("found the above lines in %s%s%s,\n", q, file, q) >"/dev/stderr";
		printf("you need to remove them first!\n") >"/dev/stderr";
		exit(1);
		}' "$MODULES_CONF")" || exit 1
	if [ "$LINES" ]; then 
		echo "needed entries already in '$MODULES_CONF'"
		echo "$LINES"
	else
		echo "adding entries to '$MODULES_CONF'"
		{
		echo "alias char-major-$MAJOR $MODULE_NAME"
		echo "options $MODULE_NAME cw_major=$MAJOR"
		} >> "$MODULES_CONF"
	fi &&
	echo "executing depmod" &&
	depmod -q -a || exit 1
	}



#############################################################################
# cleanup_modules_conf
#############################################################################
cleanup_modules_conf()
	{

	# Linux 2.4 lists the module with its file name, while Linux 2.6
	# lists it with its internal name, to handle both cases rmmod
	# is called with both names

	if [ ! -e "$MODULES_CONF" ]; then
		echo "file '$MODULES_CONF' not found"
		return
	fi
	echo "cleaning up '$MODULES_CONF'"
	awk '! /^alias char-major-'"$MAJOR $MODULE_NAME"'$/ && ! /^options '"$MODULE_NAME"' cw_major='"$MAJOR"'$/'  \
		"$MODULES_CONF" > "$MODULES_CONF.tmp" &&
	cat "$MODULES_CONF.tmp" > "$MODULES_CONF" &&
	rm -f "$MODULES_CONF.tmp" &&
	echo "removing kernel module (if loaded)" &&
	rmmod cw "$MODULE_NAME" 2>/dev/null
	echo "executing depmod" &&
	depmod -q -a || exit 1
	}



#############################################################################
# create_device_nodes
#############################################################################
create_device_nodes()
	{
	for CONTROLLER in 0 1 2 3; do
		for FLOPPY in 0 1; do
			DEVICE='/dev/cw'"$CONTROLLER"'raw'"$FLOPPY" &&
			echo "creating '$DEVICE'" &&
			mknod "$DEVICE" c "$MAJOR" $(($CONTROLLER * 64 + $FLOPPY * 32 + 31)) || exit 1
		done
	done
	}



#############################################################################
# remove_device_nodes
#############################################################################
remove_device_nodes()
	{
	for CONTROLLER in 0 1 2 3; do
		for FLOPPY in 0 1; do
			remove_file '/dev/cw'"$CONTROLLER"'raw'"$FLOPPY"
		done
	done
	}



#############################################################################
# main
#############################################################################
[ "$MAJOR" ]       || MAJOR="120"
[ "$BIN_DIR" ]     || BIN_DIR="/usr/bin"
[ "$MAN_DIR" ]     || MAN_DIR="/usr/share/man"
[ "$MODULE_DIR" ]  || MODULE_DIR="/lib/modules/$(uname -r)/kernel/drivers/char"
[ "$MODULE_NAME" ] || MODULE_NAME="cw"
if [ -z "$MODULES_CONF" ]; then
	VERSION="$(uname -r | awk 'BEGIN { FS="."; } { print($1 $2); }')"
	MODULES_CONF="/etc/modules.conf"
	[ "$VERSION" -ge "26" ] && MODULES_CONF="/etc/modprobe.conf"
fi
MAN_DIR="$MAN_DIR/man1"

if [ "$(basename $0)" = "install.sh" ]; then
	check_char_major
	cat <<-EOC > .uninstall
	MAJOR="$MAJOR"
	BIN_DIR="$BIN_DIR"
	MAN_DIR="$MAN_DIR"
	MODULE_DIR="$MODULE_DIR"
	MODULE_NAME="$MODULE_NAME"
	MODULES_CONF="$MODULES_CONF"
	EOC
	copy_file "bin/cwtool" "$BIN_DIR/cwtool"
	copy_file "module/cw.o" "$MODULE_DIR/$MODULE_NAME.o"
	copy_file "module/cw.ko" "$MODULE_DIR/$MODULE_NAME.ko"
	copy_file "doc/cwtool.1" "$MAN_DIR/cwtool.1"
	modify_modules_conf
	create_device_nodes
else
	[ -f .uninstall ] && . .uninstall
	remove_file "$BIN_DIR/cwtool"
	remove_file "$MODULE_DIR/$MODULE_NAME.o"
	remove_file "$MODULE_DIR/$MODULE_NAME.ko"
	remove_file "$MAN_DIR/cwtool.1"
	cleanup_modules_conf
	remove_device_nodes
fi
######################################################### Karsten Scheibler #
