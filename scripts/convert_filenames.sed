s/[][#!@$%^&*(){}|+=~`><?,'\":;]// g
s/\([a-z]\)\([A-Z][a-z]\)/\1_\2/ g
s/\([a-zA-Z]\)\([0-9].*\..*\)$/\1_\2/ g
s/\([0-9]\)\([a-zA-Z].*\..*\)$/\1_\2/ g
s/.*/\L&/ 
s/[ -]/_/ g
s/^[_.]*\(.*\)$/\1/ g
s/[_][_]*/_/ g
