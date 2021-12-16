#!/usr/bin/env bash
MOD_REFERAFRIEND_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/" && pwd )"

source $MOD_REFERAFRIEND_ROOT"/conf/conf.sh.dist"

if [ -f $MOD_REFERAFRIEND_ROOT"/conf/conf.sh" ]; then
    source $MOD_REFERAFRIEND_ROOT"/conf/conf.sh"
fi
