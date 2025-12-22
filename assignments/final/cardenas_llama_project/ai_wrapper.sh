#!/bin/bash

# Load configuration
if [ -f "inference.conf" ]; then
    source inference.conf
else
    echo "ERROR: inference.conf not found"
    exit 1
fi

# System prompts (choose one based on language preference)
SYSTEM_PROMPT_EN="You are a video game recommendation assistant. Recommend 5 games in table format: Name, Developer, Genre, Platform."
SYSTEM_PROMPT_RU="Ты - помощник для рекомендации видео-игр. Рекомендуй 5 игр в таблице с отступами: Название, Разработчик, Жанр, Платформа."

# Language setting: "en" or "ru" (can be set in inference.conf or here)
LANGUAGE="${LANGUAGE:-en}"

# Get system prompt based on language
get_system_prompt() {
    if [ "$LANGUAGE" = "ru" ]; then
        echo "$SYSTEM_PROMPT_RU"
    else
        echo "$SYSTEM_PROMPT_EN"
    fi
}

# File for conversation history
HISTORY_FILE="/tmp/ai_wrapper_history.txt"

# Initialize history
init_history() {
    if [ ! -f "$HISTORY_FILE" ]; then
        echo "=== Conversation History ===" > "$HISTORY_FILE"
    fi
}

# Add message to history
add_to_history() {
    local role="$1"
    local message="$2"
    echo "$role: $message" >> "$HISTORY_FILE"
    if [ "$role" = "Assistant" ]; then
        echo "---" >> "$HISTORY_FILE"
    fi
}

# Get compact history (last 2 Q&A pairs)
get_compact_history() {
    if [ ! -f "$HISTORY_FILE" ]; then
        return
    fi
    
    # Get last 8 lines (2 pairs of Q&A)
    tail -n 8 "$HISTORY_FILE" 2>/dev/null | grep -v "^===\|^---" || true
}

# Create prompt with history (properly escaped for JSON)
create_prompt_with_history() {
    local user_input="$1"
    local history=$(get_compact_history)
    local system_prompt=$(get_system_prompt)
    
    local prompt="$system_prompt"
    
    # Add history if available
    if [ -n "$history" ]; then
        if [ "$LANGUAGE" = "ru" ]; then
            prompt="$prompt\n\nПредыдущий разговор:\n$history"
        else
            prompt="$prompt\n\nPrevious conversation:\n$history"
        fi
    fi
    
    # Add current question
    if [ "$LANGUAGE" = "ru" ]; then
        prompt="$prompt\n\nТекущий вопрос: $user_input\nОтвет:"
    else
        prompt="$prompt\n\nCurrent question: $user_input\nAnswer:"
    fi
    
    echo "$prompt"
}

# Function to escape text for JSON
escape_json() {
    echo "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/\n/\\n/g' -e 's/\r/\\r/g' -e 's/\t/\\t/g'
}

# Function for local inference
local_inference() {
    local user_input="$1"
    echo "Using local model: $LOCAL_MODEL_PATH"
    
    local system_prompt=$(get_system_prompt)
    local full_prompt="System: $system_prompt\n\nUser: $user_input\n\nAssistant:"
    
    /home/linuxbrew/.linuxbrew/Cellar/llama.cpp/7030/bin/llama-cli \
        -m "$LOCAL_MODEL_PATH" \
        -c "$LOCAL_CONTEXT_SIZE" \
        -ngl "$LOCAL_NG_LAYERS" \
        -p "$full_prompt"
}

# Function for API inference using test.sh
api_inference() {
    local user_input="$1"
    
    echo "Using hurated.com API via test.sh"
    
    # Get API key
    local api_key=""
    if [ -f "key.txt" ]; then
        api_key=$(cat key.txt | tr -d '[:space:]')
    fi
    
    if [ -z "$api_key" ]; then
        echo "ERROR: No API key found in key.txt"
        echo "Please add your API key to key.txt or switch to local mode"
        return 1
    fi
    
    # Create prompt (start simple, then add history if it works)
    local prompt
    local simple_prompt=$(get_system_prompt)
    
    # Start with simple prompt to test
    prompt="$simple_prompt\n\nQuestion: $user_input\nAnswer:"
    
    # If you want to use history, uncomment this and comment the simple version above:
    # prompt=$(create_prompt_with_history "$user_input")
    
    echo "Debug: Sending prompt (first 100 chars):"
    echo "$prompt" | head -c 100
    echo "..."
    
    # Use test.sh
    local response
    if [ -f "test.sh" ] && [ -x "test.sh" ]; then
        response=$(./test.sh "$API_HOST" "$prompt" "$api_key" 2>&1)
    else
        echo "ERROR: test.sh not found or not executable"
        return 1
    fi
    
    # Debug: Show raw response
    echo "Debug: Raw response received"
    
    # Parse response
    if echo "$response" | grep -q '^{'; then
        # It's JSON
        local assistant_response
        
        # Try jq first
        if command -v jq >/dev/null 2>&1; then
            assistant_response=$(echo "$response" | jq -r '.text' 2>/dev/null)
            
            if [ -z "$assistant_response" ] || [ "$assistant_response" = "null" ]; then
                # Try other possible field names
                assistant_response=$(echo "$response" | jq -r '.response // .answer // .output // .' 2>/dev/null)
            fi
        else
            # Fallback: simple text extraction
            assistant_response=$(echo "$response" | sed -n 's/.*"text":"\([^"]*\)".*/\1/p')
            if [ -z "$assistant_response" ]; then
                assistant_response=$(echo "$response" | sed -n 's/.*"response":"\([^"]*\)".*/\1/p')
            fi
        fi
        
        if [ -n "$assistant_response" ] && [ "$assistant_response" != "null" ]; then
            # Clean up response
            assistant_response=$(echo "$assistant_response" | sed 's/\\n/\n/g')
            
            add_to_history "User" "$user_input"
            add_to_history "Assistant" "$assistant_response"
            
            echo "$assistant_response"
            return 0
        fi
    else
        # Might be plain text or HTML error
        if echo "$response" | grep -q "<html\|<!DOCTYPE"; then
            echo "ERROR: API returned HTML error page"
            echo "This usually means:"
            echo "1. Invalid or expired API key"
            echo "2. Server is down"
            echo "3. Prompt is malformed"
            echo ""
            echo "Trying with simpler English prompt..."
            
            # Try with simplest possible English prompt
            local simple_english="Recommend 5 games: Name, Developer, Genre, Platform. Question: $user_input Answer:"
            response=$(./test.sh "$API_HOST" "$simple_english" "$api_key" 2>&1)
            
            if echo "$response" | grep -q '^{'; then
                # Extract and show response
                local simple_response=$(echo "$response" | sed -n 's/.*"text":"\([^"]*\)".*/\1/p')
                if [ -n "$simple_response" ]; then
                    echo "$simple_response"
                    return 0
                fi
            fi
            
            return 1
        elif [ -n "$response" ]; then
            # Might be plain text response
            echo "$response"
            add_to_history "User" "$user_input"
            add_to_history "Assistant" "$response"
            return 0
        fi
    fi
    
    echo "ERROR: Could not get valid response from API"
    echo "Response was: $response"
    return 1
}

# Function to compile C++ client
compile_cpp_client() {
    echo "Compiling C++ client..."
    
    if [ ! -f "ai_client.cpp" ]; then
        echo "ERROR: ai_client.cpp not found"
        return 1
    fi
    
    # Check for AiAgent files
    local ai_agent_cpp=""
    local include_path=""
    
    if [ -f "ai_agent.cpp" ] && [ -f "ai_agent.h" ]; then
        ai_agent_cpp="ai_agent.cpp"
        include_path="."
    elif [ -f "src/ai_agent.cpp" ] && [ -f "src/ai_agent.h" ]; then
        ai_agent_cpp="src/ai_agent.cpp"
        include_path="src"
    else
        echo "ERROR: Could not find ai_agent.cpp and ai_agent.h"
        return 1
    fi
    
    echo "Compiling with: g++ -std=c++17 -O2 -o ai_client ai_client.cpp $ai_agent_cpp -lssl -lcrypto -I$include_path"
    
    g++ -std=c++17 -O2 -o ai_client ai_client.cpp "$ai_agent_cpp" -lssl -lcrypto -I"$include_path" 2>/tmp/compile.log
    
    if [ $? -eq 0 ]; then
        chmod +x ai_client
        echo "✓ C++ client compiled successfully"
        return 0
    else
        echo "✗ Compilation failed"
        echo "Check /tmp/compile.log for details"
        return 1
    fi
}

# Function to switch inference source
switch_source() {
    case $1 in
        local|api)
            if [ -f "inference.conf" ]; then
                sed -i "s/^INFERENCE_SOURCE=.*/INFERENCE_SOURCE=\"$1\"/" inference.conf
                source inference.conf
                echo "Switched to: $1"
            else
                echo "ERROR: inference.conf not found"
            fi
            ;;
        *)
            echo "Invalid source. Use 'local' or 'api'"
            ;;
    esac
}

# Function to switch language
switch_language() {
    case $1 in
        en|ru)
            if [ -f "inference.conf" ]; then
                if grep -q "^LANGUAGE=" inference.conf; then
                    sed -i "s/^LANGUAGE=.*/LANGUAGE=\"$1\"/" inference.conf
                else
                    echo "LANGUAGE=\"$1\"" >> inference.conf
                fi
                LANGUAGE="$1"
                echo "Language switched to: $1"
            else
                echo "ERROR: inference.conf not found"
            fi
            ;;
        *)
            echo "Invalid language. Use 'en' or 'ru'"
            ;;
    esac
}

# Function to show current configuration
show_config() {
    echo "Current configuration:"
    echo "Inference source: $INFERENCE_SOURCE"
    echo "Language: $LANGUAGE"
    
    if [ "$INFERENCE_SOURCE" = "local" ]; then
        echo "Local model: $LOCAL_MODEL_PATH"
        if [ ! -f "$LOCAL_MODEL_PATH" ]; then
            echo "WARNING: Model file not found!"
        fi
    else
        echo "API host: $API_HOST"
        if [ -f "key.txt" ]; then
            echo "API key: [loaded from key.txt]"
        else
            echo "API key: [not found - please create key.txt]"
        fi
    fi
    
    echo "History file: $HISTORY_FILE"
    
    # Check if files exist
    echo ""
    echo "File status:"
    [ -f "test.sh" ] && echo "✓ test.sh" || echo "✗ test.sh"
    [ -f "ai_client" ] && echo "✓ ai_client (C++ client)" || echo "✗ ai_client (not compiled)"
    [ -f "key.txt" ] && echo "✓ key.txt" || echo "✗ key.txt"
}

# Function to test API
test_api() {
    local host="$1"
    local prompt="$2"
    local key="$3"
    
    if [ -z "$host" ] || [ -z "$prompt" ]; then
        echo "Usage: $0 test <host> <prompt> [key]"
        echo "Examples:"
        echo "  $0 test ai-api.hurated.com \"Hello\""
        echo "  $0 test ai-api.hurated.com \"Hello\" \"\$(cat key.txt)\""
        return 1
    fi
    
    if [ -f "test.sh" ]; then
        if [ -n "$key" ]; then
            ./test.sh "$host" "$prompt" "$key"
        else
            ./test.sh "$host" "$prompt"
        fi
    else
        echo "ERROR: test.sh not found"
        return 1
    fi
}

# Function to show history
show_history() {
    if [ -f "$HISTORY_FILE" ]; then
        cat "$HISTORY_FILE"
    else
        echo "No history found"
    fi
}

# Function to clear history
clear_history() {
    rm -f "$HISTORY_FILE"
    init_history
    echo "History cleared"
}

# Function to show help
show_help() {
    echo "Usage: $0 <message|command>"
    echo ""
    echo "Commands:"
    echo "  switch [local|api]     - Switch inference source"
    echo "  language [en|ru]       - Switch language (English/Russian)"
    echo "  config                 - Show current configuration"
    echo "  test <host> <prompt> [key] - Test API directly"
    echo "  history                - Show conversation history"
    echo "  clear-history          - Clear conversation history"
    echo "  compile                - Compile C++ client"
    echo "  help                   - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 \"recommend me action games\""
    echo "  $0 switch api"
    echo "  $0 language ru"
    echo "  $0 test ai-api.hurated.com \"Hello\""
    echo "  $0 history"
    echo ""
    echo "Current mode: $INFERENCE_SOURCE, Language: $LANGUAGE"
}

# Initialize history
init_history

# Main command dispatcher
case $1 in
    "switch")
        shift
        switch_source "$@"
        ;;
    "language")
        shift
        switch_language "$@"
        ;;
    "config")
        show_config
        ;;
    "test")
        shift
        test_api "$@"
        ;;
    "history")
        show_history
        ;;
    "clear-history")
        clear_history
        ;;
    "compile")
        compile_cpp_client
        ;;
    "help"|"-h"|"--help")
        show_help
        ;;
    "")
        show_help
        ;;
    *)
        # User message - process through inference
        if [ "$INFERENCE_SOURCE" = "local" ]; then
            local_inference "$*"
        else
            api_inference "$*"
        fi
        ;;
esac