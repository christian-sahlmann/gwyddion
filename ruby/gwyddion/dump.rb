#!/usr/bin/ruby

# = Gwyddion plug-in proxy dump dumb file format handling.
#
# Written by Nenad Ocelic <ocelic _at_ biochem.mpg.de>.
# Public domain.

module Gwyddion

	class Dump < Hash
		SIZE_OF_DOUBLE= 8
		L_BRACKET= '['.unpack( 'C').first
		LINE = /^([^=]+)=(.*)\n/
		FIELD= /^([^=]+)=\[\n/
		CONV= { 'xres'=> :to_i, 'yres'=> :to_i, 'xreal'=> :to_f, 'yreal'=> :to_f}

		def initialize( filename= nil)
			super()
			self.read filename if filename
		end 

		private
		def convert!( base)
			for k, v in CONV
				fk= base+ '/'+ k
				self[ fk]= self[ fk].send( v) if v and self.key? fk 
			end 
		end 
		
		public
		# ===Read a Gwyddion plug-in proxy dump file.
		#
		# The file is returned as a dictionary of dump key, value pairs.
		#
		# Data fields are packed into dictionaries with following keys (not all has to be present):
		# * `xres', x-resolution (number of samples),
		# * `yres', y-resolution (number of samples),
		# * `xreal', real x size (in base SI units),
		# * `yreal', real y size (in base SI units),
		# * `unit-xy', lateral units (base SI, like `m'),
		# * `unit-z', value units (base SI, like `m' or `A'),
		# * `data', the data field data itself (array of floats).
		#
		# The `data' member is a raw array of floats (please see array module documentation).
		#
		# Exceptions, caused by fatal errors, are not handled -- it is up to caller to eventually handle them.
		def read( filename= @filename)						
			File.open( filename, 'rb') do | io|
				while line= io.gets
					case line
					when FIELD
						c= io.getc
						if c== L_BRACKET
							base= $1
							convert! base # known data types, incl. xres & yres
							n= self[ base+ '/xres']* self[ base+ '/yres']
							a= io.read( n* SIZE_OF_DOUBLE).unpack( "d#{n}") # network byte order would be better for portability!
							raise "Invalid file format" unless io.gets()== "]]\n"
							self[ base]= a
						else 
							io.ungetc c
							self[ $1]= "["  # does this make sense?
						end
					when LINE
						self[ $1]= $2
					else 
						raise "Can't understand input"
					end
				end
			end
			@filename= filename # all went fine, keep the name
			return self
		end
			
		# ===Write a Gwyddion plug-in proxy dump file.
		#
		# The dictionary to write is expected to follow the same conventions as
		# those returned by read(), please see its description for more.
		#
		# Exceptions, caused by fatal errors, are not handled -- it is up to
		# caller to eventually handle them.
		def write( filename= @filename)			
			File.open( filename, 'wb') do |io|
				data, desc= self.to_a.partition{| k, v| Array===v}
				for v in desc 
					io.puts '%s=%s' % v	
				end
				for k, v in data
					io.printf "%s=[\n[" % k
					io.write v.pack( 'd*')
					io.puts ']]'
				end
			end
			@filename= filename
		end
		
	end
	
end
