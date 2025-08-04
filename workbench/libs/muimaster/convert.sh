find . -name "*" -type f -exec sh -c '
    for file; do
        encoding=$(file -bi "$file" | sed -e "s/.*[ ]charset=//")
        if [ "$encoding" != "utf-8" ] && [ "$encoding" != "us-ascii" ]; then
            echo "Converting $file from $encoding to UTF-8"
            iconv -f "$encoding" -t UTF-8 "$file" > "${file}.tmp" && mv "${file}.tmp" "$file"
        fi
    done
' sh {} +
