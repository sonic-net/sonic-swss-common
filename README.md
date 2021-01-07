[![Total alerts](https://img.shields.io/lgtm/alerts/g/Azure/sonic-swss-common.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Azure/sonic-swss-common/alerts/)
[![Language grade: Python](https://img.shields.io/lgtm/grade/python/g/Azure/sonic-swss-common.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Azure/sonic-swss-common/context:python)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/Azure/sonic-swss-common.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Azure/sonic-swss-common/context:cpp)

[![Build Status](https://sonic-jenkins.westus2.cloudapp.azure.com/job/common/job/sonic-swss-common-build/badge/icon)](https://sonic-jenkins.westus2.cloudapp.azure.com/job/common/job/sonic-swss-common-build/)
[![Build Status](https://dev.azure.com/sonicswitch/build/_apis/build/status/Azure.sonic-swss-common?branchName=master)](https://dev.azure.com/sonicswitch/build/_build/latest?definitionId=9&branchName=master)

# SONiC - SWitch State Service Common Library - SWSS-COMMON

## Description
The SWitch State Service (SWSS) common library provides libraries for database communications, netlink wrappers, and other functions needed by SWSS.

## Getting Started

### Build from Source

Checkout the source:

    git clone --recursive https://github.com/Azure/sonic-swss-common


Install build dependencies:

    sudo apt-get install make libtool m4 autoconf dh-exec debhelper cmake pkg-config \
                         libhiredis-dev libnl-3-dev libnl-genl-3-dev libnl-route-3-dev \
                         libnl-nf-3-dev swig3.0 libpython2.7-dev libpython3-dev libgtest-dev \
                         libboost-dev

Install Google Test DEB package:

    cd /usr/src/gtest && sudo cmake . && sudo make

You can compile and install from source using:

    ./autogen.sh
    ./configure
    make && sudo make install

You can also build a debian package using:

    ./autogen.sh
    ./configure
    dpkg-buildpackage -us -uc -b

### Build with Google Test
1. Rebuild with Google Test
```
$ ./autogen.sh
$ ./configure --enable-debug 'CXXFLAGS=-O0 -g'
$ make clean
$ GCC_COLORS=1 make
```

2. Start redis server if not yet:
```
sudo sed -i 's/notify-keyspace-events ""/notify-keyspace-events AKE/' /etc/redis/redis.conf
sudo service redis-server start
```

3. Run unit test:
```
tests/tests
```

## Need Help?

For general questions, setup help, or troubleshooting:
- [sonicproject on Google Groups](https://groups.google.com/d/forum/sonicproject)

For bug reports or feature requests, please open an Issue.

## Contribution guide

See the [contributors guide](https://github.com/Azure/SONiC/blob/gh-pages/CONTRIBUTING.md) for information about how to contribute.

### GitHub Workflow

We're following basic GitHub Flow. If you have no idea what we're talking about, check out [GitHub's official guide](https://guides.github.com/introduction/flow/). Note that merge is only performed by the repository maintainer.

Guide for performing commits:

* Isolate each commit to one component/bugfix/issue/feature
* Use a standard commit message format:

>     [component/folder touched]: Description intent of your changes
>
>     [List of changes]
>
> 	  Signed-off-by: Your Name your@email.com

For example:

>     swss-common: Stabilize the ConsumerTable
>
>     * Fixing autoreconf
>     * Fixing unit-tests by adding checkers and initialize the DB before start
>     * Adding the ability to select from multiple channels
>     * Health-Monitor - The idea of the patch is that if something went wrong with the notification channel,
>       we will have the option to know about it (Query the LLEN table length).
>
>       Signed-off-by: user@dev.null


* Each developer should fork this repository and [add the team as a Contributor](https://help.github.com/articles/adding-collaborators-to-a-personal-repository)
* Push your changes to your private fork and do "pull-request" to this repository
* Use a pull request to do code review
* Use issues to keep track of what is going on

