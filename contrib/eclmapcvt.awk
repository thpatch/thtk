#!/usr/bin/awk -f
BEGIN {
	FS="[ \t\r]"
	#cs = "!ins_names"
	s["?"] = "!ins_names"
	s["$"] = "!gvar_names"
	s["%"] = "!gvar_names"
	s["@"] = "!timeline_ins_names"
}
#{print "["$0"]", "["$1"]", "["$2"]", "["$3"]"}
NR == 1 {
	if ($1 == "eclmap") {
		print "!eclmap"
		next
	} else {
		print "ERROR: this isn't an old-style eclmap" > "/dev/stderr"
		exit 1
	}
}
/^[ \t\r]*#/ || NF == 0 {
	print
	next
}
{
	if (NF < 3) {
		print "#FIXME#", $0
		print "WARN: Garbage on line", NR > "/dev/stderr"
		next
	}
	if ($1 !~ /^(-?[1-9][0-9]*|0)$/) {
		print "WARN: opcode `"$1"'is not numeric, line", NR > "/dev/stderr"
	}
	if ($3 !~ /^[A-Za-z_][0-9A-Za-z_]*$/ || $3 ~ /^ins_/) {
		print "WARN: identificator `"$3"' is invalid, line", NR > "/dev/stderr"
	}
	if (s[$2] == "") {
		print "#FIXME#", $0
		print "WARN: unknown signature on line", NR > "/dev/stderr"
	} else {
		if (s[$2] != cs) {
			print (cs=s[$2])
		}
		if ($2 == "$" || $2 == "%") {
			types[$1] = $2
		}
		num = $1
		$1 = $2 = ""
		print num,substr($0,3)
	}
	next
}
END {
	if (length(types) != 0) {
		print "!gvar_types"
		for (k in types) {
			print k, types[k]
		}
	}
}
