#!/usr/bin/perl -w

#
# STB Linux build system v2.1
# Copyright (C) 2011-2016 Broadcom Corporation
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# usage: tools/config.pl defaults <target>
#
# <target> examples:
#   7335b0
#   7335b0_be
#   7335b0_be-opf
#   7335b0-kgdb
#   7335b0_be-small
#   7335b0-small-nohdd
#
use strict;
use warnings;
use File::Basename;
use File::Copy;
use POSIX;
#added for testnetxl
use File::Glob ':glob';
use File::Find;
use Archive::Extract;
#end testnetxl

my %linux = ( );
my %eglibc = ( );
my %uclibc = ( );
my %busybox = ( );
my %vendor = ( );
my %rg = ( );


my $LINUXDIR = "linux";
my $RGDIR = "rg_apps";
my $ROOTFSDIR = "rootfs";

my $eglibc_defaults = "$ROOTFSDIR/defaults/config.eglibc";
my $uclibc_defaults = "$ROOTFSDIR/defaults/config.uClibc";
my $busybox_defaults = "$ROOTFSDIR/defaults/config.busybox";
my $vendor_defaults = "$ROOTFSDIR/defaults/config.vendor";
my $arch_defaults = "$ROOTFSDIR/defaults/config.arch.sample";

my $linux_defaults = "";
my $linux_new_defaults = "";
my $rg_defaults = "";

my $pwd = getcwd();
my $topdir = "$pwd/$ROOTFSDIR";

my $linux_config = "$LINUXDIR/.config";
my $eglibc_config = "$ROOTFSDIR/lib/eglibc/build/option-groups.config";
my $uclibc_config = "$ROOTFSDIR/lib/uClibc/.config";
my $busybox_config = "$ROOTFSDIR/user/busybox/.config";
my $vendor_config = "$ROOTFSDIR/config/.config";
my $arch_config = "config.arch";
my %arch_config_options = (
	"LIBCDIR"         => "eglibc",
	"MACHINE"         => "",
	"ARCH"            => "",
	"CROSS_COMPILE"   => "",
);
my $rg_config = "$RGDIR/.config";

my @patchlist = ("lttng", "newubi");
my %use_patch = ( );

my %defsuf = (
	"6328"	=> "-nohdd",  # JTAG probe
	"7118"	=> "-docsis",
	"7125"	=> "-docsis",
	"7400"	=> "-docsis",
	"7401"	=> "-docsis",
	"7403"	=> "-docsis",
	"7405"	=> "-docsis",
	"7420"	=> "-docsis",
	"7425"	=> "-docsis",
);

my ($tgt, $chip, $be, $suffix) = ("","","","");

sub read_cfg($$)
{
	my($file, $h) = @_;

	open(F, "<${file}") or die "can't open ${file}: $!";
	while(<F>) {
		if(m/^# (\S+) is not set/) {
			$$h{$1} = "n";
		} elsif(m/^(\w+)=(.+)$/) {
			$$h{$1} = $2;
		}
	}
	close(F);
}

sub write_cfg_common($$$$)
{
	my($in, $out, $h, $disable_n) = @_;
	my @outbuf = ( );

	open(IN, "<${in}") or die "can't open ${in}: $!";

	while(<IN>) {
		if(m/^# (\S+) is not set/ || m/^(\w+)=(.+)$/) {
			my $var = $1;
			my $val = $$h{$var};

			if(defined($val)) {
				if($disable_n && $val eq "n") {
					push(@outbuf, "# $var is not set\n");
				} else {
					push(@outbuf, "${var}=${val}\n");
				}
				$$h{$var} = undef;
			} else {
				push(@outbuf, $_);
			}
		} else {
			push(@outbuf, $_);
		}
	}
	close(IN);

	unlink($out);
	unless (-e dirname($out) or mkdir(dirname($out))) {
		die "can't make directory " . dirname($out);
	}
	open(OUT, ">${out}") or die "can't open ${out}: $!";

	foreach my $x (@outbuf) {
		print OUT $x;
	}

	foreach my $var (sort { $a cmp $b } keys(%$h)) {
		my $val = $$h{$var};

		if(! defined($val)) {
			next;
		}
		print OUT "${var}=${val}\n";
	}
	close(OUT);
}

sub write_cfg($$$)
{
	my($in, $out, $h) = @_;
	write_cfg_common($in, $out, $h, 1);
}

sub write_cfg_n($$$)
{
	my($in, $out, $h) = @_;
	write_cfg_common($in, $out, $h, 0);
}

sub whitelist_cfg($$)
{
	my($cfg, $whitelist) = @_;

	foreach my $x (keys(%$cfg)) {
		if(defined($$cfg{$x})) {
			if($$cfg{$x} eq "y") {
				if(! defined($$whitelist{$x})) {
					$$cfg{$x} = "n";
				}
			} elsif($$cfg{$x} eq "n") {
				if(defined($$whitelist{$x}) &&
						$$whitelist{$x} eq "y") {
					$$cfg{$x} = "y";
				}
			}
			$$whitelist{$x} = undef;
		}
	}
	foreach my $var (sort { $a cmp $b } keys(%$whitelist)) {
		my $val = $$whitelist{$var};

		if(defined($val)) {
			$$cfg{$var} = $val;
		}
	}
}

sub override_cfg($$)
{
	my($cfg, $newcfg) = @_;

	foreach my $x (keys(%$cfg)) {
		if(defined($$cfg{$x}) && defined($$newcfg{$x})) {
			$$cfg{$x} = $$newcfg{$x};
			$$newcfg{$x} = undef;
		}
	}
	foreach my $var (sort { $a cmp $b } keys(%$newcfg)) {
		my $val = $$newcfg{$var};

		if(defined($val)) {
			$$cfg{$var} = $val;
		}
	}
}

sub def($$$)
{
	my($cfg, $name, $val) = @_;

	if(! defined($$cfg{$name})) {
		$$cfg{$name} = $val;
	}
}

# This "get" function sets global variables $tgt, $chip, $be, $suffix
# and doesn't return anything.
sub get_tgt($)
{
	($tgt) = (@_);

	if(! defined($tgt)) {
		die "no target specified";
	}

	unless($tgt =~ m/^([0-9]+[a-z][0-9])(_be)?(-\S+)?$/) {
		die "invalid target format: $tgt";
	}
	($chip, $be, $suffix) = ($1, defined($2) ? 1 : 0,
		defined($3) ? $3 : "");

	if (not $suffix) {
		($suffix) = "-cms";
		($tgt) = "@_".($suffix);
	}

	#for RG need to determine which config to use based on chip and some options
	#TODO - make this a function
	if ($chip =~ /3390/) {
		if ($suffix =~ /dcm/) {
			$rg_defaults = "$RGDIR/targets/93390DCM/93390DCM";
		}
		else {
			$rg_defaults = "$RGDIR/targets/93390MWVG/93390MWVG";
		}
	}
	elsif (($chip eq "3384b0") || ($chip eq "33843b0")){
		#3384 and 33843 are the same at the application level
		$chip = "3384";
		$rg_defaults = "$RGDIR/targets/93384/93384";
		$be = 1;
	}
	elsif ($chip eq "7445d0") {
		$rg_defaults = "$RGDIR/targets/97445GW/97445GW";
	}

	# Original plan: if using a semi-multiplatform kernel we need a new
	# way to tell if the chip is MIPS or ARM.  Suggest adding new
	# metadata under include/linux/brcmstb/7*
	#
	# However, it doesn't look like that's actually happening.  Until it
	# does, we can assume that anything that has an entry under
	# include/linux/brcmstb is ARM.
	if(-d "$LINUXDIR/include/linux/brcmstb/${chip}") {
		$linux_defaults = "$LINUXDIR/arch/arm/configs/brcmstb_defconfig";
		$linux_new_defaults = "$LINUXDIR/arch/arm/configs/brcmstb_new_defconfig";
		$arch_config_options{"ARCH"} = "arm";
		return;
	}
	$linux_defaults = "$LINUXDIR/arch/mips/configs/bcm${chip}_defconfig";
	if(-e $linux_defaults) {
		$linux_new_defaults = $linux_defaults;
		$linux_new_defaults =~ s/defconfig$/new_defconfig/;
		$arch_config_options{"ARCH"} = "mips";
		return;
	}

	print "\n";
	print "ERROR: No Linux configuration for $chip\n";
	print "Attempted to open: $LINUXDIR/arch/arm/configs/brcmstb_defconfig,\n";
	print "                   $linux_defaults\n";
	print "\n";
	exit 1;
}

################################################
# sub expand_modifiers($implies, $list)
#
# DESCRIPTION:
#   Expands modifiers according to the rules given
#   by the %implies.  Eliminates duplicate modifiers.
# PARAMS:
#   $implies is a hash ref of implied modifier actions.
#   $list is a hyphenated string of successive modifiers.
# RETURNS:
#   Final hyphenated string of modifiers.
################################################
sub expand_modifiers($$)
{
	my $implies = shift;
	my $t = shift;
	my @a = split /-/, $t;
	my ($nsubs, $iters, $MAX_ITERS) = (0,0,15);

	# Expand out modifiers.
	do {
		$nsubs = 0;
		for (my $i=$#a; $i>=0; $i--) {
			my $v = $a[$i];
			if ($implies->{$v}) {
				splice(@a,$i,1,@{$implies->{$v}},uc($v));
				$nsubs++;
			}
		}
	} while ($nsubs && $iters++ < $MAX_ITERS);

	die "Error: modifiers have a mutually recursive definition; fix \%implies!"
		if ($iters >= $MAX_ITERS);

	# Now we uniquify elements while preserving order from right.
	my @aa = reverse map { lc } @a;
	@a = ();
	my %h;
	while (@aa) {
		my $x = shift @aa;
		next if $h{$x};
		$h{$x} = 1;
		unshift @a, $x;
	}
	return join('-',@a);
}

sub get_chiplist()
{
	my @defs = glob("$LINUXDIR/include/linux/brcmstb/7*
	                $LINUXDIR/arch/mips/configs/bcm*_defconfig");
	my @out = ( );

	foreach (@defs) {
		if(m/([0-9]+[a-z][0-9])/) {
			push(@out, $1);
		}
	}

	return(@out);
}

sub set_opt_common($$)
{
	my($file, $settings) = @_;
	my %h;

	read_cfg($file, \%h);

	foreach my $x (@$settings) {
		if($x !~ /^(\w+)=(.+)$/) {
			die "Invalid setting: $x";
		}
		my($key, $val) = ($1, $2);
		if(defined($h{$key})) {
			if($h{$key} eq $val) {
				print "$key: no change\n";
			} else {
				print "$key: change from '$h{$key}' to ".
					"'$val'\n";
			}
		} else {
			print "$key: add new option with value '$val'\n";
		}
		$h{$key} = $val;
	}
	return \%h;
}

sub set_opt($$)
{
	my($file, $settings) = @_;
	write_cfg($file, $file, set_opt_common($file, $settings));
}

sub set_opt_n($$)
{
	my($file, $settings) = @_;
	write_cfg_n($file, $file, set_opt_common($file, $settings));
}

sub test_opt($$)
{
	my($file, $settings) = @_;
	my %h;
	my $result = 0;

	read_cfg($file, \%h);

	foreach my $key (@$settings) {
		if(!defined($h{$key}) || ($h{$key} eq 'n')) {
			$result = 1;
		}
	}

	exit $result;
}

sub gen_arch_config($)
{
	my $arch = shift;
	my $out = "";

	unlink($arch_config);
	open(IN, "<$arch_defaults") or
		die "can't open $arch_defaults: $!";
	while(<IN>) {
		foreach my $key (keys %arch_config_options) {
			my $searchstr = "\@CONFIG_PL_$key\@";
			s/$searchstr/$arch_config_options{$key}/;
		}
		$out .= $_;
	}
	close(IN);
	open(OUT, ">$arch_config") or
		die "can't open $arch_config: $!";
	print OUT "$out";
	close(OUT);
}


#
# COMMAND HANDLERS
#
# If you add one of these, make sure to add appropriate entries to
# %cmd_table.
#

sub cmd_badcmd($)
{
	my ($cmd) = @_;
	die "unrecognized command: $cmd";
}

sub cmd_defaults($)
{
	my ($cmd) = @_;
	get_tgt(shift @ARGV);

	# clean up the build system if switching targets
	# "quick" mode (skip distclean) is for testing only
	if(-e ".target" && $cmd ne "quickdefaults") {
		open(F, "<.target") or die "can't read .target";
		my $oldtgt = <F>;
		close(F);

		$oldtgt =~ s/[\r\n]//g;
		if($tgt ne $oldtgt) {
			print "\n";
			print "Can't switch from target $oldtgt to $tgt\n";
			print "Please run \"make distclean\"\n";
			print "\n";
			exit 1;
		}
		unlink(".target");
	}

	unlink($linux_config);
	system(qq(make -C $LINUXDIR ARCH=$arch_config_options{"ARCH"} ) . basename($linux_defaults));

	read_cfg($linux_config, \%linux);
	read_cfg($eglibc_defaults, \%eglibc);
	read_cfg($uclibc_defaults, \%uclibc);
	read_cfg($busybox_defaults, \%busybox);
	read_cfg($vendor_defaults, \%vendor);
        if (-d $RGDIR) {
           read_cfg($rg_defaults, \%rg);
        }

	# set chip, e.g. CONFIG_BCM7445A0=y

	my $capchip = $chip;
	$capchip =~ tr/a-z/A-Z/;

	foreach my $x (keys(%linux)) {
		if ($x =~ m/^CONFIG_BCM7[0-9]{3,}(?!_)/) {
			$linux{$x} = "n";
		}
	}
	$linux{'CONFIG_BCM'.$capchip} = 'y';

	# set architecture (only for uClibc)

	if($arch_config_options{"ARCH"} eq "arm") {
		my %uclibc_o;

		read_cfg("$ROOTFSDIR/defaults/override.uClibc-arm", \%uclibc_o);
		override_cfg(\%uclibc, \%uclibc_o);
	} else {
		# The kernel CMA components are currently arm-only.
		$vendor{"CONFIG_USER_CMATEST"} = "n";
	}

	# basic hardware support

	# MOCA
	$vendor{"CONFIG_USER_MOCA_MOCA1"} = "n";
	$vendor{"CONFIG_USER_MOCA_NONE"} = "y";
	$vendor{"CONFIG_USER_MOCA_MOCA2"} = "n";
	$vendor{"CONFIG_USER_MOCA_GEN1"} = "n";
	$vendor{"CONFIG_USER_MOCA_GEN2"} = "n";
	$vendor{"CONFIG_USER_MOCA_GEN3"} = "n";

	if (defined($linux{"CONFIG_PM"})) {
		$vendor{"CONFIG_USER_BRCM_PM"} = "y";
	}

	if (defined($linux{"CONFIG_I2C"})) {
		$vendor{"CONFIG_USER_I2C_TOOLS"} = "y";
	}

	if(($chip eq "7445d0")) {
		$linux{"CONFIG_GPIO_BRCMCM"} = "n";
		$linux{"CONFIG_BCM_PININIT"} = "n";
		$vendor{"CONFIG_USER_BRCM_MBOX_ASSIST"} = "n";
		$vendor{"CONFIG_USER_BRCM_BOOT_ASSIST"} = "y";
		$vendor{"CONFIG_USER_BRCM_BOOT_ASSIST_LITE"} = "n";
		$vendor{"CONFIG_USER_BRCM_RPROGSTORE"} = "n";
		$vendor{"CONFIG_USER_BRCM_RNONVOL"} = "n";
	}

	if($chip =~ /3390/) {
		$vendor{"CONFIG_USER_BRCM_BOOT_ASSIST_LITE"} = "n";
		if(defined($linux{"CONFIG_BCM_BOOT_ASSIST"})) {
		$vendor{"CONFIG_USER_BRCM_BOOT_ASSIST_A0"} = "n";
		$vendor{"CONFIG_USER_BRCM_BOOT_ASSIST"} = "y";
		$vendor{"CONFIG_USER_BRCM_HYP"} = "y";
		$vendor{"CONFIG_USER_WDMD"} = "y";
	    } else {
		$vendor{"CONFIG_USER_BRCM_BOOT_ASSIST_A0"} = "n";
		$vendor{"CONFIG_USER_BRCM_BOOT_ASSIST"} = "n";
		$vendor{"CONFIG_USER_BRCM_HYP"} = "n";
	    }

	    if(defined($linux{"CONFIG_BCM_MBOX"})) {
		$vendor{"CONFIG_USER_BRCM_MBOX_ASSIST"} = "y";
	    }

	    if($chip =~ /3390/) {
			$vendor{"CONFIG_USER_BRCM_WPSCTL"} = "n";
			#No hypervisor support in 3390
			$linux{"CONFIG_ARM_PSCI"} = "n";
			#No STB/Nexus in 3390
			$linux{"CONFIG_CMA"} = "n";
			$vendor{"CONFIG_USER_BRCM_DT"} = "y";
			$vendor{"CONFIG_USER_BRCM_THREAD"} = "y";
	    }
	} else {
	    $vendor{"CONFIG_USER_BRCM_BOOT_ASSIST"} = "n";
	    $vendor{"CONFIG_USER_BRCM_BOOT_ASSIST_LITE"} = "n";
	    $vendor{"CONFIG_USER_BRCM_BOOT_ASSIST_A0"} = "n";
	    $vendor{"CONFIG_USER_BRCM_MBOX_ASSIST"} = "n";
	    $vendor{"CONFIG_USER_BRCM_HYP"} = "n";
	    $vendor{"CONFIG_USER_BRCM_WPSCTL"} = "n";
	    $vendor{"CONFIG_USER_BRCM_DT"} = "n";
	}

	# set default modifiers for each chip

	my $shortchip = $chip;
	if($shortchip =~ /^\d{3,}/) {
		$shortchip =~ s/[^\d].*//;
	}

	if(defined($defsuf{$shortchip})) {
		$suffix = $defsuf{$shortchip}.$suffix;
	}

	# The %implies hash indicates what modifiers imply other modifiers.
	# If X implies Y-Z, then the modications will be applied in this
	# order: Y,Z,X.
	my %implies = ('pal' => 'small-nonet-nousb-nohdd',
		       'ikos' => 'small-kdebug-nousb-nomtd-nohdd',
		       'kgdb' => 'kdebug',
	    );

	# Munge the '%implies' hash so that its values become array refs.
	map { $_ = [split /-/] } values %implies;

	my $old_suffix = $suffix;
	$suffix = expand_modifiers(\%implies, $suffix);

	# print "info: '$old_suffix' expanded to '$suffix'.\n";

	my (%vendor_w, %busybox_w, %linux_o, %vendor_o, %busybox_o);

	# allow stacking more than one modifier (e.g. -small-nohdd-nousb)
	while(defined($suffix) && ($suffix ne "")) {
		if($suffix !~ m/^-([^-]+)(-\S+)?/) {
			print "\n";
			print "ERROR: Invalid modifier '$suffix' in '$tgt'\n";
			print "\n";
			exit 1;
		}
		(my $mod, $suffix) = ($1, $2);

		if($mod eq "small") {

			# reduced footprint (-small builds)

			$uclibc{"PTHREADS_DEBUG_SUPPORT"} = "n";
			$uclibc{"DODEBUG"} = "n";
			$uclibc{"DOSTRIP"} = "y";

			# disable all but a few features

			read_cfg("$ROOTFSDIR/defaults/whitelist.vendor-small",
				\%vendor_w);
			read_cfg("$ROOTFSDIR/defaults/whitelist.busybox-small",
				\%busybox_w);

			whitelist_cfg(\%vendor, \%vendor_w);
			whitelist_cfg(\%busybox, \%busybox_w);

			$linux{"CONFIG_NETWORK_FILESYSTEMS"} = "n";
			$linux{"CONFIG_INPUT"} = "n";
			$linux{"CONFIG_VT"} = "n";
		} elsif($mod eq "ikos") {

			# IKOS pre-tapeout emulation (internal Broadcom use)
			# 'ikos' implies '-small-kdebug-nousb-nomtd-nohdd'

			$linux{"CONFIG_BRCM_DEBUG_OPTIONS"} = "y";
			$linux{"CONFIG_BRCM_IKOS"} = "y";
			$linux{"CONFIG_BRCM_IKOS_DEBUG"} = "y";
			$linux{"CONFIG_BRCM_FORCED_DRAM0_SIZE"} = "32";
			$linux{"CONFIG_BRCM_FORCED_DRAM1_SIZE"} = "0";
			$linux{"CONFIG_BRCM_PM"} = "n";
			$vendor{"CONFIG_USER_BRCM_PM"} = "n";
		} elsif($mod eq "kgdb") {

			# KGDB debugging (implies -kdebug)
			# 'kgdb' implies '-kdebug'

			$linux{"CONFIG_KGDB"} = "y";
			$linux{"CONFIG_KGDB_SERIAL_CONSOLE"} = "y";
			$linux{"CONFIG_KGDB_TESTS"} = "n";
			$linux{"CONFIG_KGDB_LOW_LEVEL_TRAP"} = "n";
			$linux{"CONFIG_KGDB_KDB"} = "n";
		} elsif($mod eq "gdb") {

			# Native GDB CLI on target (warning: GPLv3 code)

			$vendor{"CONFIG_USER_GDB_GDB"} = "y";
		} elsif($mod eq "opf") {

			# Oprofile - non-debug kernel with CONFIG_OPROFILE set

			$linux{"CONFIG_PROFILING"} = "y";
			$linux{"CONFIG_OPROFILE"} = "y";
			$linux{"CONFIG_JBD2_DEBUG"} = "n";
			$linux{"CONFIG_MARKERS"} = "n";
			$linux{"CONFIG_NET_DROP_MONITOR"} = "n";
			$linux{"CONFIG_FTRACE_STARTUP_TEST"} = "n";
			$linux{"CONFIG_RING_BUFFER_BENCHMARK"} = "n";

			$vendor{"CONFIG_USER_PROFILE_OPROFILE"} = "y";
		} elsif($mod eq "kdebug") {

			# Kernel debug info + extra sanity checks

			read_cfg("$ROOTFSDIR/defaults/override.linux-kdebug", \%linux_o);
			override_cfg(\%linux, \%linux_o);
			def(\%linux, "CONFIG_BRCM_IKOS", "n");
			def(\%linux, "CONFIG_KGDB", "n");
		} elsif($mod eq "netfilter") {

			# Enable netfilter and iptables

			read_cfg("$ROOTFSDIR/defaults/override.linux-netfilter",
				\%linux_o);
			override_cfg(\%linux, \%linux_o);
			$vendor{"CONFIG_USER_IPTABLES_IPTABLES"} = "y";
		} elsif($mod eq "ipv6") {

			# Enable IPv6

			read_cfg("$ROOTFSDIR/defaults/override.linux-ipv6",
				\%linux_o);
			override_cfg(\%linux, \%linux_o);

			# FIXME: missing dependencies
			# $vendor{"CONFIG_USER_DHCPCV6_DHCPCV6"} = "y";

			$uclibc{"UCLIBC_HAS_IPV6"} = "y";
			$eglibc{"OPTION_EGLIBC_ADVANCED_INET6"} = "y";
			$busybox{"CONFIG_FEATURE_IPV6"} = "y";
			$busybox{"CONFIG_PING6"} = "y";
			$busybox{"CONFIG_UDHCPC6"} = "y";
		} elsif($mod eq "docsis") {

			# enable tftp server for DOCSIS firmware download

			$busybox{"CONFIG_UDPSVD"} = "y";
			$busybox{"CONFIG_TFTPD"} = "y";
		} elsif($mod eq "nousb") {
			$linux{"CONFIG_USB"} = "n";
		} elsif($mod eq "nomtd") {
			$vendor{"CONFIG_USER_MTDUTILS"} = "n";
			$linux{"CONFIG_MTD"} = "n";
			# JFFS2, UBIFS depend on CONFIG_MTD
			$linux{"CONFIG_SQUASHFS"} = "n";
		} elsif($mod eq "nohdd") {

			# Disable all hard disk support (SATA or USB)

			$busybox{"CONFIG_MKSWAP"} = "n";
			$busybox{"CONFIG_SWAPONOFF"} = "n";
			$busybox{"CONFIG_FDISK"} = "n";
			$vendor{"CONFIG_USER_GPTFDISK_GDISK"} = "n";
			$vendor{"CONFIG_USER_GPTFDISK_SGDISK"} = "n";
			$vendor{"CONFIG_USER_E2FSPROGS_E2FSCK_E2FSCK"} = "n";
			$vendor{"CONFIG_USER_E2FSPROGS_MISC_MKE2FS"} = "n";
			$vendor{"CONFIG_USER_E2FSPROGS_MISC_TUNE2FS"} = "n";
			$linux{"CONFIG_ATA"} = "n";
			$linux{"CONFIG_SCSI"} = "n";

			# disable all non-MTD filesystems
			$linux{"CONFIG_EXT4_FS"} = "n";
			$linux{"CONFIG_JBD2"} = "n";
			$linux{"CONFIG_FUSE_FS"} = "n";
			$linux{"CONFIG_ISO9660_FS"} = "n";
			$linux{"CONFIG_UDF_FS"} = "n";
			$linux{"CONFIG_FAT_FS"} = "n";
			$linux{"CONFIG_VFAT_FS"} = "n";
			$linux{"CONFIG_MSDOS_FS"} = "n";
			$linux{"CONFIG_NLS"} = "n";
		} elsif($mod eq "nonet") {
			# busybox compile fails with no brctl
			# $busybox{"CONFIG_BRCTL"} = "n";
			$busybox{"CONFIG_FTPGET"} = "n";
			$busybox{"CONFIG_FTPPUT"} = "n";
			$busybox{"CONFIG_HOSTNAME"} = "n";
			$busybox{"CONFIG_IFCONFIG"} = "n";
			$busybox{"CONFIG_IP"} = "n";
			$busybox{"CONFIG_NETSTAT"} = "n";
			$busybox{"CONFIG_PING"} = "n";
			$busybox{"CONFIG_ROUTE"} = "n";
			$busybox{"CONFIG_TELNET"} = "n";
			$busybox{"CONFIG_TELNETD"} = "n";
			$busybox{"CONFIG_TFTP"} = "n";
			$busybox{"CONFIG_UDHCPC"} = "n";
			$busybox{"CONFIG_VCONFIG"} = "n";
			$busybox{"CONFIG_WGET"} = "n";
			$busybox{"CONFIG_ZCIP"} = "n";
			$linux{"CONFIG_NET"} = "n";

			$vendor{"CONFIG_USER_MOCA_NONE"} = "y";
			$vendor{"CONFIG_USER_MOCA_MOCA1"} = "n";
			$vendor{"CONFIG_USER_MOCA_MOCA2"} = "n";
		} elsif($mod eq "lttng") {

			# Enable LTTng

			$use_patch{'lttng'} = 1;

			read_cfg("$ROOTFSDIR/defaults/override.linux-lttng", \%linux_o);
			override_cfg(\%linux, \%linux_o);

			$vendor{"CONFIG_USER_LTT_CONTROL"} = "y";

			$busybox{"CONFIG_FEATURE_FIND_PRUNE"} = "y";
			$busybox{"CONFIG_FEATURE_FIND_PATH"} = "y";
		} elsif($mod eq "android") {

			# Enable Android
			if (! -e "$LINUXDIR/Documentation/android.txt") {
				print "Not an android kernel. ";
				print "Build android targets only with the android kernel repo.\n";
				die("");
			}

		} elsif($mod eq "newubi") {

			# UBI/UBIFS backport from the mainline MTD tree

			$use_patch{'newubi'} = 1;
		} elsif($mod eq "lxc") {

			# Enable LXC containers

			read_cfg("$ROOTFSDIR/defaults/override.linux-lxc", \%linux_o);
			override_cfg(\%linux, \%linux_o);

			$vendor{"CONFIG_USER_LXC_LXC"} = "y";
			$vendor{"CONFIG_LIB_LIBCAP"} = "y";

			$busybox{"CONFIG_GETOPT"} = "y";
			$busybox{"CONFIG_FEATURE_GETOPT_LONG"} = "y";
			$busybox{"CONFIG_ID"} = "y";
		} elsif($mod eq "uvc") {

			# Enable UVC - USB Video Class

			read_cfg("$ROOTFSDIR/defaults/override.linux-uvc", \%linux_o);
			override_cfg(\%linux, \%linux_o);
		} elsif($mod eq "xfs") {

			# Enable XFS file system

			$linux{"CONFIG_XFS_FS"} = "y";
			$linux{"CONFIG_XFS_QUOTA"} = "n";
			$linux{"CONFIG_XFS_POSIX_ACL"} = "y";
			$linux{"CONFIG_XFS_RT"} = "y";
			$linux{"CONFIG_XFS_DEBUG"} = "n";
			$vendor{"CONFIG_USER_XFS_XFSPROGS"} = "y";
		} elsif($mod eq "perf") {

			# perf - performance counters and function tracer

			$linux{"CONFIG_HAVE_PERF_EVENTS"} = "y";
			$linux{"CONFIG_PERF_EVENTS"} = "y";
			$linux{"CONFIG_HW_PERF_EVENTS"} = "y";
			$linux{"CONFIG_DEBUG_PERF_USE_VMALLOC"} = "n";
			$linux{"CONFIG_NET_DROP_MONITOR"} = "n";
			$linux{"CONFIG_EVENT_POWER_TRACING_DEPRECATED"} = "y";
			$linux{"CONFIG_FUNCTION_GRAPH_TRACER"} = "y";
			$linux{"CONFIG_DYNAMIC_FTRACE"} = "y";
			$linux{"CONFIG_FUNCTION_PROFILER"} = "n";
			$linux{"CONFIG_FTRACE_STARTUP_TEST"} = "n";
			$linux{"CONFIG_RING_BUFFER_BENCHMARK"} = "n";
			$linux{"CONFIG_FUNCTION_TRACER"} = "y";

			$vendor{"CONFIG_USER_PERF"} = "y";
			$vendor{"CONFIG_USER_PERF_TOOLS"} = "y";
			$vendor{"CONFIG_USER_PROCPS_SYSCTL"} = "y";

			$busybox{"CONFIG_EXPAND"} = "y";
		} elsif($mod eq "eglibc") {
			$arch_config_options{"LIBCDIR"} = "eglibc";
		} elsif($mod eq "uclibc" || $mod eq "uClibc") {
			$arch_config_options{"LIBCDIR"} = "uClibc";
		} elsif($mod eq "psci") {
			$linux{"CONFIG_ARM_PSCI"} = "y";
		} elsif($mod eq "dcm") {

			# dcm - Slimmmed-down configuration

			$vendor{"CONFIG_USER_MOCA_NONE"} = "y";
			$vendor{"CONFIG_USER_MOCA_MOCA1"} = "n";
			$vendor{"CONFIG_USER_MOCA_MOCA2"} = "n";

			read_cfg("$ROOTFSDIR/defaults/override.linux-dcm", \%linux_o);
			override_cfg(\%linux, \%linux_o);
		} elsif($mod eq "nocms") {
			if(-d $RGDIR) {
				print "mod='$mod'\n";
				print "building without router application\n";
				$rg{"BUILD_BRCM_CMS"} = "n";
				$rg{"BUILD_ZEBRA"} = "n";
				$rg{"BUILD_IPV6"} = "n";
				$rg{"BUILD_SNMP"} = "n";
				$rg{"BUILD_AXIS2C"} = "n";
			}
		} elsif($mod eq "lattice") {
			if(-d $RGDIR) {
				print "mod='$mod'\n";
				print "building lattice router application\n";
				$rg{"BUILD_WLMNGR2"} = "n";
				$rg{"BRCM_DRIVER_WIRELESS"} = "n";
				$rg{"BUILD_WIFI_VISUALIZATION"} = "n";
				$rg{"SUPPORT_NO_WIFI_CMS"} = "y";
				$busybox{"CONFIG_UDHCPD"} = "y";
			}
		} elsif($mod eq "mixedap") {
			print "buiding mixedap WLAN driver\n";
			$rg{"BUILD_WLAN_mixedap"} = "y";
			$rg{"BUILD_WLAN_nicap"} = "n";
		} elsif($mod eq "wifi") {
			print "disable WLEROUTER and WLEROUTER_RFAP\n";
                        $rg{"BUILD_WLMNGR2"} = "y";
                        $rg{"BRCM_DRIVER_WIRELESS"} = "y";
			$rg{"BUILD_WLEROUTER"} = "n";
		} elsif($mod eq "nicap") {
			print "buiding nicap WLAN driver\n";
			$rg{"BUILD_WLAN_nicap"} = "y";
			$rg{"BUILD_WLAN_mixedap"} = "n";
                } elsif($mod eq "testnetxl") {
                        print "Buiding for NetXL test - this will also untar the NetXL release pacakge if it exists\n";
                        $linux{"CONFIG_BCM_NETXL"} = "y";
                        $rg{"BUILD_NETXL"} = "y";
                        my @netxl_tars = glob('../UCG*netxl*.tar.gz');
                        my $netxl_tar = $netxl_tars[0];
                        if (defined $netxl_tar ) {
                           #found a tarball
                           print "Untarring $netxl_tar to ..\n";
                           my $ae = Archive::Extract->new(archive => $netxl_tar);
                           $ae->extract( to => '..') or die "Failed extracting NetXl tarball $netxl_tar failed: $ae->error";
                           copy("../rg/rg_apps/bcmdrivers/broadcom/netxl/netxl.o_shipped", "./rg_apps/bcmdrivers/broadcom/netxl/netxl.o") or die "Failed copying netxl.o $!";
                        }
                        else {
                           #no tarball, is there source?
                           my $found = 0;
                           find ( sub { if (/netxl.c/) { $found = 1; print "found $_ \n"; } }, ".");
                           if ($found == 1) {
                              #yes there is source so we are OK
                           }
                           else{
                              print "Could not find netxl source and there is no tarball at ../UCG*netxl*.tar.gz exiting! \n";
                              exit 1;
                           }
                        }
		} elsif($mod eq "nologin") {
		} elsif($mod eq "pon") {
		} elsif($mod eq "cert") {
		    $rg{"BUILD_CERTIFICATION"} = "y";
			$linux{"CONFIG_BRCM_CERTIFICATION"} = "y";
		} elsif($mod eq "bridge") {
			$rg{"BUILD_BRIDGE"} = "y";
		} elsif($mod eq "cms") {
			print "Add CMS as suffix $tgt\n";
		} else {
			print "\n";
			print "ERROR: Unrecognized suffix '$mod' in '$tgt'\n";
			print "\n";
			exit 1;
		}
	}

	# overrides based on endian/arch setting

	if($be == 0) {
		$linux{"CONFIG_CPU_LITTLE_ENDIAN"} = "y";
		$linux{"CONFIG_CPU_BIG_ENDIAN"} = "n";

		$uclibc{"ARCH_LITTLE_ENDIAN"} = "y";
		$uclibc{"ARCH_WANTS_LITTLE_ENDIAN"} = "y";
		$uclibc{"ARCH_BIG_ENDIAN"} = "n";
		$uclibc{"ARCH_WANTS_BIG_ENDIAN"} = "n";
		$arch_config_options{"MACHINE"} = $arch_config_options{"ARCH"} eq "arm" ? "arm" : "mipsel";
	} else {
		$linux{"CONFIG_CPU_LITTLE_ENDIAN"} = "n";
		$linux{"CONFIG_CPU_BIG_ENDIAN"} = "y";

		$uclibc{"ARCH_LITTLE_ENDIAN"} = "n";
		$uclibc{"ARCH_WANTS_LITTLE_ENDIAN"} = "n";
		$uclibc{"ARCH_BIG_ENDIAN"} = "y";
		$uclibc{"ARCH_WANTS_BIG_ENDIAN"} = "y";
		$arch_config_options{"MACHINE"} = $arch_config_options{"ARCH"} eq "arm" ? "armeb" : "mips";
	}

	$arch_config_options{"CROSS_COMPILE"} = qq($arch_config_options{"MACHINE"}-linux-);
	if($arch_config_options{"LIBCDIR"} eq "uClibc") {
		if($arch_config_options{"ARCH"} eq "arm") {
			$arch_config_options{"CROSS_COMPILE"} .= "uclibceabi-";
		} else {
			$arch_config_options{"CROSS_COMPILE"} .= "uclibc-"
		}
	} else {  # (e)glibc
		if($arch_config_options{"ARCH"} eq "arm") {
			$arch_config_options{"CROSS_COMPILE"} .= "gnueabihf-";
		} else {
			$arch_config_options{"CROSS_COMPILE"} .= "gnu-";
		}
	}
	$uclibc{"CROSS_COMPILER_PREFIX"} = '"'.$arch_config_options{"CROSS_COMPILE"}.'"';
	$busybox{"CONFIG_CROSS_COMPILER_PREFIX"} = '"'.$arch_config_options{"CROSS_COMPILE"}.'"';
	$linux{"CONFIG_CROSS_COMPILE"} = '"'.$arch_config_options{"CROSS_COMPILE"}.'"';

	gen_arch_config($arch_config_options{"ARCH"});

	# misc

	$busybox{"CONFIG_PREFIX"} = "\"$topdir/romfs\"";

	my $CC = qq($arch_config_options{"CROSS_COMPILE"}gcc);
	my $sysroot = `$CC --print-sysroot`;
	if(WEXITSTATUS($?) != 0) {
		die "can't invoke $CC to find sysroot (bad toolchain in PATH?)";
	}
	$sysroot =~ s/\s//g;
	$busybox{"CONFIG_SYSROOT"} = "\"$sysroot\"";
	$uclibc{"KERNEL_HEADERS"} = "\"$sysroot/usr/include\"";

	if ($eglibc{"OPTION_EGLIBC_NSSWITCH"} eq "n") {
		$eglibc{"OPTION_EGLIBC_NSSWITCH_FIXED_CONFIG"} =
			"$topdir/lib/eglibc/nss/fixed-nsswitch.conf";
		$eglibc{"OPTION_EGLIBC_NSSWITCH_FIXED_FUNCTIONS"} =
			"$topdir/lib/eglibc/nss/fixed-nsswitch.functions";
	}

	# apply/reverse kernel patches

	my $cwd = getcwd();
	chdir($LINUXDIR) or die;

	foreach my $x (@patchlist) {
		if(defined($use_patch{$x})) {
			if(! -e "patch/.applied-$x") {
				system("patch -p2 < patch/$x.patch");

				my $ret = WEXITSTATUS($?);
				if($ret != 0) {
					die "patch exited with code $ret";
				}
				open(F, ">patch/.applied-$x") or die;
				close(F);
			}
		} else {
			if(-e "patch/.applied-$x") {
				system("patch -R -p2 < patch/$x.patch");

				my $ret = WEXITSTATUS($?);
				if($ret != 0) {
					die "patch exited with code $ret";
				}
				unlink("patch/.applied-$x") or die;
			}
		}
	}
	chdir($cwd) or die;

	open(F, ">.target") or die "can't write .target: $!";
	print F "$tgt\n";
	close(F);

	open(F, ">.arch") or die "can't write .arch: $!";
	print F qq($arch_config_options{"ARCH"}\n);
	close(F);

	# write out the new configuration
	write_cfg($linux_defaults, $linux_new_defaults, \%linux);
	write_cfg_n($eglibc_defaults, $eglibc_config, \%eglibc);
	write_cfg($uclibc_defaults, $uclibc_config, \%uclibc);
	write_cfg($busybox_defaults, $busybox_config, \%busybox);
	write_cfg($vendor_defaults, $vendor_config, \%vendor);
	if (-d $RGDIR) {
		write_cfg($rg_defaults, $rg_config, \%rg);
	}

	# fix up the kernel defconfig (due to CONFIG_BCM* munging)
	system(qq(make -C $LINUXDIR ARCH=$arch_config_options{"ARCH"} ) . basename($linux_new_defaults));
	unlink($linux_new_defaults);
}

sub cmd_save_defaults()
{
	get_tgt(shift @ARGV);

	read_cfg($linux_config, \%linux);
	read_cfg($eglibc_config, \%eglibc);
	read_cfg($uclibc_config, \%uclibc);
	read_cfg($busybox_config, \%busybox);
	read_cfg($vendor_config, \%vendor);
	if (-d $RGDIR) {
		read_cfg($rg_config, \%rg);
	}

	write_cfg($linux_config, $linux_config, \%linux);
	system("make -C $LINUXDIR savedefconfig");
	copy("$LINUXDIR/defconfig", $linux_defaults);

	write_cfg_n($eglibc_config, $eglibc_defaults, \%eglibc);
	write_cfg($uclibc_config, $uclibc_defaults, \%uclibc);
	write_cfg($busybox_config, $busybox_defaults, \%busybox);
	write_cfg($vendor_config, $vendor_defaults, \%vendor);
	if (-d $RGDIR) {
		write_cfg($rg_config, $rg_defaults, \%rg);
	}
}

sub cmd_initramfs()
{
	read_cfg($linux_config, \%linux);

	$linux{"CONFIG_BLK_DEV_INITRD"} = "y";

	$linux{"CONFIG_BLK_DEV_RAM"} = "y";
	$linux{"CONFIG_BLK_DEV_RAM_COUNT"} = "16";
	$linux{"CONFIG_BLK_DEV_RAM_SIZE"} = "8192";
	$linux{"CONFIG_BLK_DEV_RAM_BLOCKSIZE"} = "1024";
	$linux{"CONFIG_BLK_DEV_XIP"} = "n";
	$linux{"CONFIG_PROBE_INITRD_HEADER"} = "n";

	$linux{"CONFIG_INITRAMFS_SOURCE"} = "\"$topdir/romfs ".
		"$topdir/misc/initramfs.dev\"";
	$linux{"CONFIG_INITRAMFS_ROOT_UID"} = getuid();
	$linux{"CONFIG_INITRAMFS_ROOT_GID"} = getgid();

	$linux{"CONFIG_INITRAMFS_COMPRESSION_NONE"} = "y";
	$linux{"CONFIG_INITRAMFS_COMPRESSION_GZIP"} = "n";
	$linux{"CONFIG_INITRAMFS_COMPRESSION_BZIP2"} = "n";
	$linux{"CONFIG_INITRAMFS_COMPRESSION_LZMA"} = "n";
	$linux{"CONFIG_INITRAMFS_COMPRESSION_LZO"} = "n";

	write_cfg($linux_config, $linux_config, \%linux);
}

sub cmd_noinitramfs()
{
	read_cfg($linux_config, \%linux);
	$linux{"CONFIG_INITRAMFS_SOURCE"} = '""';
	write_cfg($linux_config, $linux_config, \%linux);
}

sub cmd_chiplist()
{
	foreach (get_chiplist()) {
		print "$_\n";
	}
}

sub cmd_buildlist()
{
	foreach (get_chiplist()) {
		print "$_\n";
		# 73xx, 74xx generally need BE builds
		# 70xx, 71xx, 72xx generally do not
		if(m/^7[34]/ || m/^7038/) {
			print "${_}_be\n";
		}
	}
}

sub cmd_linux()
{
	set_opt($linux_config, \@ARGV);
}

sub cmd_busybox()
{
	set_opt($busybox_config, \@ARGV);
}

sub cmd_eglibc()
{
	set_opt_n($eglibc_config, \@ARGV);
}

sub cmd_uclibc()
{
	set_opt($uclibc_config, \@ARGV);
}

sub cmd_vendor()
{
	set_opt($vendor_config, \@ARGV);
}

sub cmd_test_linux()
{
	test_opt($linux_config, \@ARGV);
}

sub cmd_test_busybox()
{
	test_opt($busybox_config, \@ARGV);
}

sub cmd_test_uclibc()
{
	test_opt($uclibc_config, \@ARGV);
}

sub cmd_test_vendor()
{
	test_opt($vendor_config, \@ARGV);
}

sub cmd_openwrt_arch($)
{
	get_tgt(shift @ARGV);

	$arch_config_options{"LIBCDIR"} = "eglibc";
	$arch_config_options{"MACHINE"} = $arch_config_options{"ARCH"};

	if($arch_config_options{"ARCH"} eq "arm") {
		$arch_config_options{"CROSS_COMPILE"} .= "arm-linux-gnueabihf-";
	} else {
		$arch_config_options{"CROSS_COMPILE"} .= "mips-linux-gnu-";
	}

	gen_arch_config($arch_config_options{"ARCH"});

	open(F, ">.target") or die "can't write .target: $!";
	print F "$tgt\n";
	close(F);

	open(F, ">.arch") or die "can't write .arch: $!";
	print F qq($arch_config_options{"ARCH"}\n);
	close(F);
}

my %cmd_table = (
	"defaults"         => \&cmd_defaults,
	"quickdefaults"    => \&cmd_defaults,
	"save_defaults"    => \&cmd_save_defaults,
	"initramfs"        => \&cmd_initramfs,
	"noinitramfs"      => \&cmd_noinitramfs,
	"chiplist"         => \&cmd_chiplist,
	"buildlist"        => \&cmd_buildlist,
	"linux"            => \&cmd_linux,
	"busybox"          => \&cmd_busybox,
	"eglibc"           => \&cmd_eglibc,
	"uclibc"           => \&cmd_uclibc,
	"vendor"           => \&cmd_vendor,
	"test_linux"       => \&cmd_linux,
	"test_uclibc"      => \&cmd_uclibc,
	"test_busybox"     => \&cmd_busybox,
	"test_vendor"      => \&cmd_vendor,
	"openwrt_arch"     => \&cmd_openwrt_arch,
);

#
# MAIN
#

my $cmd = shift @ARGV;
if(! defined($cmd)) {
	die "usage: config.pl <cmd>\n";
}
($cmd_table{$cmd} || \&cmd_badcmd)->($cmd);
