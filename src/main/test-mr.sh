#!/bin/bash
here=$(dirname "$0")
[[ "$here" = /* ]] || here="$PWD/$here"
echo ""
echo "==> Part II"
(cd "$here" && sh ./test-wc.sh > /dev/null)
echo ""
echo "==> Part V (inverted index)"
(cd "$here" && sh ./test-ii.sh > /dev/null)
export GOPATH="$here/../../"
echo ""
echo "==> Part I"
go test -run Sequential mapreduce/...
echo ""
echo "==> Part III"
go test -run TestParallel mapreduce/...
echo ""
echo "==> Part IV"
go test -run Failure mapreduce/...

rm "$here"/mrtmp.* "$here"/diff.out
