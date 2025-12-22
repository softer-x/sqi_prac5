#!/usr/bin/env bash

# Simple API test script for ai_wrapper.sh
# Returns JSON response with "text" field

if [ $# -lt 2 ]; then
    echo '{"text": "Error: Usage: test.sh <host> <prompt> [api_key]"}'
    exit 1
fi

HOST="$1"
PROMPT="$2"
API_KEY="$3"

# Escape the prompt for JSON (simple escaping)
PROMPT_ESCAPED=$(echo "$PROMPT" | sed 's/"/\\"/g')

# Build the JSON payload
JSON_PAYLOAD="{\"prompt\": \"$PROMPT_ESCAPED\"}"

# Debug output (optional)
# echo "Sending to: https://$HOST/api/generate" >&2
# echo "Payload: $JSON_PAYLOAD" >&2

if [ -n "$API_KEY" ]; then
    # With API key
    curl -s -X POST "https://$HOST/api/generate" \
         -H "Content-Type: application/json" \
         -H "x-api-key: $API_KEY" \
         -d "$JSON_PAYLOAD" \
         --connect-timeout 30 \
         --max-time 60
else
    # Without API key
    curl -s -X POST "https://$HOST/api/generate" \
         -H "Content-Type: application/json" \
         -d "$JSON_PAYLOAD" \
         --connect-timeout 30 \
         --max-time 60
fi

# Add a newline at the end
echo ""