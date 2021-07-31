#!/usr/bin/perl

use lib '/mnt/panzer/rahbera/ChampSim/scripts';
use warnings;
use Math::Combinatorics;

my @feature_names = ("PC", "Offset", "Delta", "Address", "PC_Offset", "PC_Address", "PC_Page", "PC_Path", "Delta_Path", "Offset_Path", "PC_Delta", "PC_Offset_Delta", "Page", "PC_Path_Offset", "PC_Path_Offset_Path", "PC_Path_Delta", "PC_Path_Delta_Path", "PC_Path_Offset_Path_Delta_Path", "Offset_Path_PC", "Delta_Path_PC");

############# CHANGE ONLY THESE PARAMS #############
my @selected_features = ("0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19");
my @num_combs = ("1", "2");
my $tilings = 3;
my $tiles = 128;
####################################################

foreach $num_comb (@num_combs)
{
	my $num_tilings = join(",", ($tilings)x$num_comb);
	my $num_tlies = join(",", ($tiles)x$num_comb);
	my $hash_types = join(",", (2)x$num_comb);
	my $enable_tiling_offset = join(",", (1)x$num_comb);
	my $weights = join(",", (1)x$num_comb);
	my $cmdline = "--le_featurewise_num_tilings=$num_tilings --le_featurewise_num_tiles=$num_tlies --le_featurewise_hash_types=$hash_types --le_featurewise_enable_tiling_offset=$enable_tiling_offset --le_featurewise_feature_weights=$weights";

	my @combinations = combine($num_comb, @selected_features);

	print "#Combinations using $num_comb features\n";
	foreach my $combo (@combinations)
	{
		my @combo2 = @$combo;
		$sel_feat_ids = join(",", @combo2);
		$sel_feat_names = join("+", map { $feature_names[$_] } @combo2);
		$exp_name = "scooby_${sel_feat_names}_${tilings}x${tiles}";
		print "${exp_name}  \$(BASE) \$(SCOOBY) --le_featurewise_active_features=${sel_feat_ids} $cmdline\n";
	}
	print "\n";
}
