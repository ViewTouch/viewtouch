#!/bin/sh

for file in `find . -name '*.cc' -o -name '*.hh'`
do
	sed -e "s/[[:<:]]char[[:>:]]/genericChar/g" <$file
	#mv $file.new $file
done
