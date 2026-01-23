cd ./chromium/src
export PATH=$(realpath ../../depot_tools):$PATH
autoninja -C out/Default explain chrome