#!/usr/bin/env ruby

# A very simple Gwyddion plug-in example in Ruby.

# Written by Nenad Ocelic <ocelic _at_ biochem.mpg.de>.
# Public domain.

$:.push(ENV['GWYPLUGINLIB'] + '/ruby')
require "gwyddion/dump"
include Gwyddion

# Plug-in information.
RUN_MODES= 'noninteractive', 'with_defaults'
PLUGIN_INFO= [ "invert_ruby", "/_Test/Value Invert (Ruby)"]

def register(args)
	puts PLUGIN_INFO, RUN_MODES.join(' ')
end 

def run( args)
	run_mode= args.shift
	RUN_MODES.member? run_mode or raise "Invalid run mode"
	
	dump= Dump.new args.shift
	a= dump[ '/0/data']
	
	n= a.length
	mirror= a.min + a.max
	for i in (0 ... n)
		a[ i]= mirror- a[ i]
	end
	dump.write # filename optional unless changed
end 

fn= ARGV.shift
send fn.intern, ARGV
