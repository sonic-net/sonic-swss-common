export CGO_LDFLAGS := -lswsscommon -lhiredis
export CGO_CXXFLAGS := -I../ -I../common/ -w -Wall -fpermissive

GO ?= /usr/local/go/bin/go
SWIG ?= /usr/bin/swig
RM=rm -f
LN=ln

SWIG_FLAG = -go -cgo -c++ -intgosize 64
ifeq ($(CONFIGURED_ARCH),arm64)
SWIG_FLAG += -DSWIGWORDSIZE64
endif

.PHONY: all check clean

all:
	$(SWIG) $(SWIG_FLAG) -I/usr/include/swss/ swsscommon.i

check:
	sudo CGO_LDFLAGS="$(CGO_LDFLAGS)" CGO_CXXFLAGS="$(CGO_CXXFLAGS)" $(GO) test

clean:
	$(RM) swsscommon.go *.cxx

