#!/usr/bin/env ruby
# vim: ts=2 sw=2 noet:

def process(filename)
	data = File.open(filename).read

	imagename = filename.tr('.', '_')
	puts "static const guint8 #{imagename}[] = {"
	s = ""
	data.each_byte { |b|
		case b.chr
		when '\\'           # escape backslash
			s += '\\\\'
		when '"'            # escape doublequote
			s += '\\"'
		when /[\x20-\x7e]/  # already printable
			s += b.chr
		else                # escape as octal
			s += '\%03o' % b 
		end

		if s.length > 72
			puts '    "' + s + '"'
			s = ''
		end
	}
	if s.length > 0
		puts '    "' + s + '"'
	end
	puts "};"
	puts
end

%w(logjam_ljuser.png logjam_ljcomm.png logjam_twuser.png logjam_protected.png logjam_private.png).each { |img|
	process(img)
}

