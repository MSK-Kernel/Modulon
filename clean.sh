#/bin/sh

START=$(date +%s%N)
log()
{
        NOW=$(date +%s%N)
        ELAPSED=$((NOW - START))
        SECONDS=$((ELAPSED / 1000000000))
        MILLISECONDS=$((ELAPSED / 1000000 % 1000))
        printf "[%d:%03d] %s\n" "$SECONDS" "$MILLISECONDS" "$1"
}

rm -rf bin
log "SH rm -rf bin"
echo "Successfully cleaned output"
