use ExtUtils::MakeMaker;
use Config '%Config';

#BEGIN {unshift @INC, 'utils'}
use Math::PariBuild;
use strict;

ExtUtils::MakeMaker::WriteEmptyMakefile(), return 1 if $common::parilib;

*MY::top_targets  = \&xMY::top_targets;		# will break WriteEmptyMakefile
*MY::const_config = \&xMY::const_config;

my $main_paridir = $common::main_paridir;
die "Please start Makefile.PL at toplevel, not in libPARI"
  unless $main_paridir;

if ($main_paridir =~ m,^([a-z]:)?[/\\],i) {
  $main_paridir = $common::main_paridir;
} else {
  $main_paridir = "../$common::main_paridir";
}

my $pari_version = $common::pari_version;
my $machine = $common::machine;

if (defined $machine) {
  print "...Via command-line: processor family `$machine'\n";
} else {
  $machine = find_machine_architecture;
}
if ( $machine eq 'port'			# Set by find_machine_architecture()
     and defined($Config{longsize}) and $Config{longsize} == 8 ) {
  #$mycflags .= " -DLONG_IS_64BIT";	# Moved to BuildPari...
}

# Choose which assembler files to use
my $asmarch = choose_and_report_assembler($machine, $pari_version);
# $machine used as a coarse-grain factor below, $asmarch as a fine-grain
$machine = 'port' if $asmarch eq 'none';

# This part is based on analysing Makefile.SH in */kernel/* subdirectories.

my $asscmd;
if ($machine =~ /sparc|alpha|hppa/) {
  $asscmd = '$(AS) $(ASFLAGS)';	#  $(CCCDLFLAGS)?  sparc is non-relocatable...
} else {
  $asscmd = '$(CCCMD) $(CCCDLFLAGS)';
}

my $as_message = not_gnu_as();
my $Using_gnu_as = not $as_message;
print STDERR
  "...Assembler is " . ($Using_gnu_as ? "" : "not ") . "GNU assembler\n";

my $asmcpp_define = '';
if ($Using_gnu_as and $^O eq 'solaris' and $asmarch =~ /^sparc/) {
  $asmcpp_define = '-DNO_UNDERSCORE';
  $asmcpp_define .= ' -D__GNUC__' if $Config{gcc};
}

# Which files to compile and how to build levels1/2 kernel
my $kernels = kernel_files($asmarch, $pari_version, $Using_gnu_as, $main_paridir);
kernel_fill_data($kernels, \ my %kernel);

if ($common::src eq 'src64') {{ # 64-bit Win32 is tricky
  warn(<<EOD), last if -d "$main_paridir/src64";
Directory $main_paridir/src64 present.
  I assume it was correctly generated.
EOD
  die <<EOD . "  Remove this dir to generate it again!" if -d "$main_paridir/src64a";
Directory $main_paridir/src64a present.
  I assume there was an error generating $main_paridir/src64!
EOD
  Math::PariBuild::filter_for_ll_all($main_paridir, 'src64a');
  rename("$main_paridir/src64a", "$main_paridir/src64") or die "Error renaming `$main_paridir/src64a' to `$main_paridir/src64'!";
}}

# remaining kernel C files
my $kernel_dir = '$(PARI_DIR_SRC)/kernel';
my $mp	       = "$kernel_dir/none/mp.c";
my $mpinl      = "$kernel_dir/none/level1.c"; # Non-inlined versions of inlines
my $mpinlh     = "$kernel_dir/none/level1.h"; # Put as a dependence
if ($pari_version >= 2003000) {
  $mpinl = "$kernel_dir/none/mpinl.c";
  $mpinlh = '';
}
my $thread_engine = $common::thread_engine;

(my $ARCH = $^O) =~ s/^MSWin32$/mingw/;
my @cfiles =			# Remaining C files
  grep !m(/ ( plot (?!port|gnuplot)
	    | ix86 | version | mpin | dummy | mt/(?!(mt|$thread_engine)\.)
	    | gp_(rl|init) | kerntest | whatnow | gp\. )
	 )x,	<$main_paridir/src/*/*.c>, # kernel C files deeper in the tree
		<$main_paridir/src/systems/$ARCH/*.c>;
push @cfiles, grep -f, map "$main_paridir/src/graph/$_.c", qw(plottty plotps)
  if $pari_version >= 2009000;		# exist and work at 2.9.0, fail on 2.7.6
push @cfiles, "$main_paridir/src/graph/plotnull.c"
  unless grep m(/graph/(plotgnuplot|plotps)\.c), @cfiles;
my @cat_cfiles =			# Remaining kernel C files
  grep !m(/ ( level \d+ | mp ) )x,  <$main_paridir/src/kernel/none/*.c>;


@cfiles = grep ! m(/test/[^/]+\.c$), @cfiles;
map s/^\Q$main_paridir\/src/\$(PARI_DIR_SRC)/, @cfiles;
my %cfiles = map {m,/([^/.]*)\.c$,i; ($1,$_)} @cfiles; # By basename

my $gcflags = $cfiles{plotgnuplot} ? '-DPLOT_IS_TUNABLE' : '';	# for versions 2.3.*
my $mycflags;
my $noexp2 = '';		# Not used any more - but would be nice!
my $libs="-lm";

if ($Config{osname} eq 'solaris') {
#  @sc_dirs = '/opt/SUNWspro/lib' if -d '/opt/SUNWspro/lib';
#  @sc_dirs = </opt/SUNWspro/SC*/lib> unless @sc_dirs;
  #warn "Cannot find SUNWspro dirs, needed for -lsunmath, using NOEXP2.\n"
  #  unless @sc_dirs;
#  if (@sc_dirs) {
#    $libs .= " -L$sc_dirs[-1] -lsunmath";
#  } else {
    $noexp2 = 1;
#  }
} elsif ($Config{osname} eq 'sunos') {
  # $machine = 'sparcv8_super' if $Config{myuname} =~ /\bsun4m\b/o;
  $mycflags .= ' -DULONG_NOT_DEFINED';
}

# We remove optimize options, since Perl probably knows them
# and one can specify "OPTIMIZE=-O2 -m486" on the command line

my $extra_inc = extra_includes($main_paridir);

my $inc = '';
if ($Config{cc} eq 'gcc') {
  # We do not put -ansi to avoid versioncflags
  $mycflags="";			#$mycflags="-O2 -g";
  if ($Config{osname} eq 'os2' and not $extra_inc =~ /os2(?!\S)/) {
    # 2.1.3 does not include dlctl.h
    $mycflags .= ' -DRTLD_LAZY=0 -DRTLD_GLOBAL=0';
    $inc .= ' -I $(PERL_INC)';
  }
  # This is actually a bug in PARI:
  $mycflags .= " -DaOPT__vecsort0=aOPTas__Og" if $Config{gccversion} !~ /^[1-3]\./ and $pari_version =~ /^200500[01]/;
} elsif ($machine eq 'hppa') {
  $mycflags="-Aa -DHPPA";	#$mycflags="-O -Aa -DHPPA";
} elsif ($machine eq 'ix86') {
  $mycflags="";			#$mycflags="-O2 -m486";
} else {
  $mycflags="";			#$mycflags="-O";
}

# These are not needed with newer versions of PARI
# $mycflags .= ' -Dshifts=pari_shifts'; # Conflict with libnsl

$mycflags .= ' -Derr=pari_err' if $pari_version < 2003000; # On linux it can get a wrong dynamic loading; disappeared in 2.2.12
my $add_ar_flags = '';

if ($machine eq 'alpha') {
  #$mycflags .= " -DLONG_IS_64BIT";	# Moved to BuildPari...
} elsif ($Config{osname} eq 'solaris') {
  $mycflags .= " -DSOLARIS";
} elsif ($Config{osname} eq 'os2') {
  $noexp2 = 1;
  $mycflags .= ' -DMALLOC_PROCS -DHAS_STRICMP';
  $add_ar_flags = '-p64';	# Need big library. 32 is OK without debugging
} elsif ($Config{osname} eq 'linux') {
  $noexp2 = 1;
}

# Since Perl's Config has no direct checks for stat() and opendir(), use presence of OpenGroup's header files
$mycflags .= ' -DHAS_STAT'	if $Config{i_sysstat};
$mycflags .= ' -DHAS_OPENDIR'	if $Config{i_dirent};

if ($Config{gccversion}
    and not $common::do_configure
    and not ($asmarch eq 'alpha'
	     and $Config{gccversion} !~ /^(3\.)|(2\.95\.3.*)/)) {
  $mycflags .= " -DASMINLINE";
}
$mycflags .= ' -DGCC_INLINE' if $Config{gccversion};

my @obj_files = map { "$_\$(OBJ_EXT)" } keys(%cfiles), qw(mp mpinl);
push @obj_files, 'kernel$(OBJ_EXT)' if $kernel{converted1};
push @obj_files, 'kernel2$(OBJ_EXT)' if $kernel{converted2};

$mycflags .= " -DDYNAMIC_PLOTTING";
# OMF build needs no underscores:
$mycflags .= " -D__NO_AOUT" if $^O eq 'os2' and !$OS2::is_aout;

# This is done in Makefile in GP/PARI, and in paricfg.h without do_configure
$mycflags .= ' -DDL_DFLT_NAME=NULL' if $common::do_configure;

# Fix for broken gcc 2.95 with -ffast-math
my @opt;
my $opt = $Config{optimize};

if (defined $Config{gccversion} and $Config{gccversion} =~ /^2\.9[46]/
    and $opt =~ s/-ffast-math// and not "@ARGV" =~ /\bOPTIMIZE=/) {
  @opt = ( OPTIMIZE => $opt );
}

build_funclists($main_paridir, $pari_version);


my $Using_ms_vc = ($^O =~ /win32/i and $Config{cc} =~ /cl/i);
my $Using_Borland = ($^O =~ /win32/i and $Config{cc} =~ /\bbcc/i);

WriteMakefile(
    NAME	=> 'Math::PARI::libPARI',
    LINKTYPE	=> 'static',
    LIBS	=> $libs,
    OBJECT	=> join(' ', @obj_files),
    @opt,
    macro	=> {
		    PARI_DIR	  => $main_paridir,
		    PARI_DIR_SRC  => "$main_paridir/$common::src",
		    ADD_AR_OPT	  => $add_ar_flags,
		    CPP		  => $Config{cpprun},		# May be a reason for problems with _hiremainer _overflow
		    # CPPMINUS	  => $Config{cppminus},
		    ASSCMD	  => $asscmd,
				# Extra defines when CPPing assembler files:
		    ASSCPPDEFINE  => $asmcpp_define,
				# Use this when assembling:
		    ASFLAGS	  => assembler_flags_via($machine, $as_message),
				# defines used when assembling:
		    ASSDEFINE	  => ($Using_gnu_as ? '' : '$(DEFINE)'),
		    MY_CC_PRE_TARGET => ($Using_ms_vc
					 ? '-Fo'
					 : ($Using_Borland
					    ? '-o': '-o $(MY_EMPTY_STR)')),
		    MY_AR_PRE_TARGET => ($Using_ms_vc
					 ? '-out:'
					 : ($Using_Borland
					    ? '' : 'cr $(MY_EMPTY_STR)')),
		    MY_AR_OBJECT => ($Using_Borland
				     ? '$(OBJECT:^"+")'
				      : '$(OBJECT)'),
		    MY_EMPTY_STR  => '', # The rest to simplify remote debug:
		    USED_ASMARCH  => $asmarch,
		    CFG_CCCDLFLAGS => $Config{cccdlflags},
		   },
    DEFINE	=> "$gcflags$mycflags",
    INC		=> $extra_inc . ' -I $(PARI_DIR_SRC)/headers -I $(PARI_DIR_SRC)/graph -I .'
			. $inc,
    C		=> \@cfiles,
    SKIP	=> [qw( distclean test dist makeaperl xs_o static)],
    clean	=> {FILES =>
		    'libPARI$(LIB_EXT) parimt.h pariinl.h kernel1.s kernel2.s paricfg.h mp.c kernel1.c mpinl.h parilvl0.h parilvl1.h '},
#   ($ExtUtils::MakeMaker::VERSION >= 6.3002 ? (LICENSE  => 'gpl_2' ) : ()),
);

# Create preprocessed copies of assembler files (GNU as does not run cpp);
# Create .h file with inlining rules and an object file mpinl with
# non-inlined versions of normally inlined functions.
# Assemble assembler files.
# Compile C files from a *different* directory.
# Create a library;
sub xMY::top_targets {
  my $converted = '';
  my @inlines2 = inline_headers2($asmarch, $pari_version, $main_paridir);
  my $inlines_cond = $inlines2[0];
  my $inlines = $inlines2[1];
  $converted = <<EOC if $kernel{convert};
$kernel{converted1}: $kernel{file1} $kernel{header1}
	\$(CPP) -I . \$(INC) -I $kernel{dir1} \$(DEFINE) \$(ASSCPPDEFINE) $kernel{file1} | perl -ne "s/%\\s+/%/g; print unless /^\\s*#/" > \$@

kernel2.s: $kernel{file2}
	\$(CPP) -I . \$(INC) -I $kernel{dir1} \$(DEFINE) \$(ASSCPPDEFINE) $kernel{file2} | perl -ne "s/%\\s+/%/g; print unless /^\\s*#/" > \$@

EOC

  my @inline;
  -f $_ and push @inline, $_ for "$main_paridir/src/kernel/$asmarch/asm0.h";
  push @inline, "$main_paridir/src/kernel/sparcv8_micro/asm0-common.h"
    if @inline and $asmarch =~ /^sparcv8_(super|micro)$/;	# inspect MakeLVL0.SH in subdirectories
  my (@no_asm, @do_asm);
  for my $fn (@inline) {
    open my $f, '<', $fn or die "Cannot open $fn for read: $!";
    while (<$f>) {
      next unless /^(NO)?ASM\s+(.*)/;
      my $arr = $1 ? \@no_asm : \@do_asm;
      push @$arr, map "$main_paridir/src/kernel/none/$_.h", split /\s+/, $2;
    }
  }
  for my $list (\@inline, \@no_asm, \@do_asm) {
    s(^\Q$main_paridir/src/\E)(\$(PARI_DIR_SRC)/) for @$list;
  }
  my $create_inl250 = '';
  my $d_tune = (-r './tune.h' ? '.' : '$(PARI_DIR_SRC)/kernel/none');
  my @extra = ($pari_version >= 2009000 ? '$(PARI_DIR_SRC)/kernel/none/divll_pre.h' : ());
  $create_inl250 = <<EOC if $pari_version >= 2004000;	# XXX needed with 2005000; when did it appear???
L0MODS=@inline @no_asm @do_asm @extra

parilvl0.h: \$(L0MODS)
	\$(PERL) -pe1 pre-noinline.h @do_asm post-noinline.h @inline @no_asm @extra > \$\@

L1OBJS=\$(PARI_DIR_SRC)/kernel/none/int.h \$(PARI_DIR_SRC)/kernel/none/level1.h
DIR_TUNE=$d_tune

parilvl1.h: \$(L1OBJS) \$(DIR_TUNE)/tune.h
	\$(PERL) -lpe0 \$(DIR_TUNE)/tune.h \$(L1OBJS) > \$\@

mpinl.h: parilvl1.h parilvl0.h
	\$(PERL) -lpe0 parilvl0.h parilvl1.h > \$\@

EOC
  $create_inl250 .= <<EOC if $pari_version >= 2007000;	# XXX needed with 2007000; when did it appear???
THREAD_ENGINE = $thread_engine

parimt.h:
	\$(PERL) -lpe0 \$(PARI_DIR_SRC)/mt/\$(THREAD_ENGINE).h > \$\@

EOC
  my $create_mp = '';
  if ($pari_version >= 2002005) {
    my $mp_pre = $mp;
    $mp = 'mp.c';
    my $kern = "$kernel_dir/none";
    my @dep			# Copied from src/kernel/none/MakeLVL1.SH 2.2.10
      = "$kern/mp.c $kern/cmp.c $kern/gcdll.c $kern/ratlift.c $kern/gcd.c $kern/invmod.c $kern/mp_indep.c $kern/add.c";
    -f "$main_paridir/src/kernel/$_" and push @dep, "$kernel_dir/$_" for "none/gcdext.c";	# When did it appear?  Was:  if $pari_version > 2004000
    $create_mp = <<EOS;
mp.c: @dep
	\$(PERL) -pe1 @dep > \$@
EOS
  }
  my $prepend_inl = "\t\$(PERL) -wle0 > \$@\n"; # So that `>>' works...
  for my $list ($inlines, $inlines_cond) {
    s(^\Q$main_paridir/src/\E)(\$(PARI_DIR_SRC)/) for @$list;
  }
  if (@$inlines_cond) {
    $prepend_inl = <<EOI;
	\$(PERL) -wle "print q(#ifndef ASMINLINE)" > \$@
	\$(PERL) -pe1 @$inlines_cond		  >> \$@
	\$(PERL) -wle "print q(#endif)"		  >> \$@
EOI
  }
  my $deps_h250 = ($pari_version > 2004000 && 'mpinl.h');	# Actually, this is definitely needed in 2.5.0.  Do not know when this appeared
  $deps_h250 .= ' parimt.h' if $pari_version >= 2007000;
  my $deps_h = "pariinl.h paricfg.h $deps_h250";
  return '
all :: libPARI$(LIB_EXT)

#  Makefile.PL skips sections: distclean test dist makeaperl xs_o static
#  We need to restore targets which may be called from outside (e.g., 5.22 starts to call test_dynamic)

static ::       libPARI$(LIB_EXT)


test:

test_:

test_static:

test_dynamic:

testdb:

testdb_static:

testdb_dynamic:


libPARI$(LIB_EXT): $(OBJECT)
	-$(RM_F) libPARI$(LIB_EXT)
	$(AR) $(ADD_AR_OPT) $(MY_AR_PRE_TARGET)libPARI$(LIB_EXT) $(MY_AR_OBJECT)
	$(RANLIB) libPARI$(LIB_EXT)

' . qq{
$create_mp
pariinl.h: @$inlines_cond @$inlines
$prepend_inl	\$(PERL) -pe1 @$inlines			  >> \$@

$converted$create_inl250

mp\$(OBJ_EXT): $mp pariinl.h $deps_h250
	\$(CCCMD) \$(CCCDLFLAGS) \$(DEFINE) \$(MY_CC_PRE_TARGET)\$@ $mp

kernel\$(OBJ_EXT): $kernel{converted1} pariinl.h $deps_h250
	\$(ASSCMD) \$(ASSDEFINE) \$(MY_CC_PRE_TARGET)\$@ $kernel{converted1}

kernel2\$(OBJ_EXT): $kernel{converted2} pariinl.h $deps_h250
	\$(ASSCMD) \$(ASSDEFINE) \$(MY_CC_PRE_TARGET)\$@ $kernel{converted2}

mpinl\$(OBJ_EXT): $mpinl $mpinlh pariinl.h $deps_h250
	\$(CCCMD) \$(CCCDLFLAGS) \$(DEFINE) \$(MY_CC_PRE_TARGET)\$@ $mpinl

} .
    join "\n", map { <<EOF } sort keys %cfiles;
$_\$(OBJ_EXT): $cfiles{$_} $deps_h
	\$(CCCMD) \$(CCCDLFLAGS) \$(DEFINE) \$(MY_CC_PRE_TARGET)\$@ $cfiles{$_}
EOF
}


# Rewrite pic option to PIC in CCCDLFLAGS,
# Why manipulate LD* when no ld is done here (static build only?)
sub MY::const_config
{
 my $self = shift;
 my $flags = $self->{'CCCDLFLAGS'}; # Tmp var needed with Perl4 !
 $flags =~ s/(-[fK]?\s*)pic\b/${1}PIC/;
 $flags =~ s/-KPIC/-K PIC/;	# Apparently needed on Solaris...
 $self->{'CCCDLFLAGS'} = $flags;
	# This line has no effect; handling of CCFLAGS is very complicated...  See below, MY::cflags.
 $self->{'CCFLAGS'} =~ s/-D_FORTIFY_SOURCE=\d+\b//;	# XXXX On NetBSD; breaks input_method->fgets() of es.c of 2.3.5
 if ($^O eq 'MSWin32' && $Config{'ccflags'} =~ /-DPERL_OBJECT/)
  {
   $self->{'LDFLAGS'} =~ s/-(debug|pdb:\w+)\s+//g;
   $self->{'LDDLFLAGS'} =~ s/-(debug|pdb:\w+)\s+//g;
  }
 return $self->MM::const_config;
}

sub MY::cflags
{
  my $self = shift;
  my $txt = $self->MM::cflags;
  $txt =~ s/(?<!\S)-D_FORTIFY_SOURCE=\d+\b//;		# XXXX On NetBSD; breaks input_method->fgets() of es.c of 2.3.5
  if ($ENV{AUTOMATED_TESTING} and $^O eq 'netbsd') {	# Warnings overflow the testing infrastructure
#    $txt .= ' -Wno-discarded-qualifiers' if $Config{gccversion};
    $txt =~ s/(?<!\S)-W(all|extra)\b//g;
  }
  return $txt;
}
