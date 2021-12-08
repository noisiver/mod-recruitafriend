#!/usr/bin/env bash
MOD_RECRUITAFRIEND_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/" && pwd )"

source $MOD_RECRUITAFRIEND_ROOT"/conf/conf.sh.dist"

if [ -f $MOD_RECRUITAFRIEND_ROOT"/conf/conf.sh" ]; then
    source $MOD_RECRUITAFRIEND_ROOT"/conf/conf.sh"
fi
