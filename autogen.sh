#!/bin/bash

touch NEWS README AUTHORS COPYING ChangeLog
libtoolize --force --copy &&
autoreconf --force --install -I m4
rm -Rf autom4te.cache

