# ANBS User Guide

## Table of Contents
1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Interface Overview](#interface-overview)
4. [AI Commands](#ai-commands)
5. [Memory System](#memory-system)
6. [File Analysis](#file-analysis)
7. [Health Monitoring](#health-monitoring)
8. [Configuration](#configuration)
9. [Keyboard Shortcuts](#keyboard-shortcuts)
10. [Best Practices](#best-practices)
11. [Troubleshooting](#troubleshooting)

## Introduction

The AI-Native Bash Shell (ANBS) revolutionizes the command-line experience by integrating artificial intelligence directly into your terminal. ANBS provides a split-screen interface where traditional bash commands coexist with AI-powered assistance, creating an intelligent development environment.

### Key Features
- **Split-Screen Interface**: Traditional bash on the left, AI chat on the right
- **Native AI Commands**: @vertex, @memory, @analyze built into the shell
- **Conversation Memory**: Persistent semantic search across all interactions
- **Real-time Health Monitoring**: Visual indicators for AI agent status
- **Sub-50ms Response Times**: Optimized for lightning-fast AI interactions
- **Enterprise Security**: Sandboxed execution with role-based permissions

## Getting Started

### First Launch
When you start ANBS, you'll see a split-screen interface:

```
┌─ Terminal (70%) ────────────┐┌─ AI Chat (30%) ──────┐
│ $ pwd                       ││ 🤖 Vertex AI Ready   │
│ /home/user/projects         ││                      │
│ $ ls                        ││ Type @vertex help    │
│ file1.txt  script.py        ││ to get started       │
│ $                           ││                      │
└─────────────────────────────┘└──────────────────────┘
┌─ Health Monitor ────────────────────────────────────┐
│ CPU: ██████████ 45%  Memory: ████████   78%        │
│ AI Agent Status: ✅ ONLINE  Response Time: 23ms     │
└─────────────────────────────────────────────────────┘
```

### Basic Interaction
1. Use the left panel for traditional bash commands
2. Use AI commands (@vertex, @memory, @analyze) in either panel
3. Monitor system health in the bottom panel
4. Switch focus between panels with Tab or Ctrl+A

## Interface Overview

### Panel Layout
- **Terminal Panel (Left)**: Full bash compatibility with integrated AI commands
- **AI Chat Panel (Right)**: Dedicated AI conversation space with history
- **Health Monitor (Bottom)**: Real-time system and AI agent status

### Visual Indicators
- 🤖 **AI Agent Online**: Green indicator when Vertex is responding
- ⚠️ **AI Agent Warning**: Yellow indicator for degraded performance
- ❌ **AI Agent Offline**: Red indicator when AI services are unavailable
- 📊 **Performance Metrics**: CPU, memory, and response time displays

### Panel Management
```bash
# Switch to AI chat panel
Ctrl+A

# Toggle split-screen mode
Ctrl+S

# Resize panels
Ctrl+Left/Right

# Minimize/maximize panels
Ctrl+M
```

## AI Commands

### @vertex - Main AI Assistant

The @vertex command provides access to the primary AI assistant with various capabilities.

#### Basic Usage
```bash
# Simple query
@vertex "What files are in this directory?"

# Code assistance
@vertex "Explain this Python function" < script.py

# Health check
@vertex --health
```

#### Advanced Options
```bash
# Specify model
@vertex --model gpt-4 "Complex code analysis request"

# Set timeout
@vertex --timeout 30 "Long-running analysis task"

# Stream response
@vertex --stream "Tell me about machine learning"

# Save to file
@vertex "Generate documentation" > output.md
```

#### Use Cases
- **Code Review**: `@vertex "Review this function for bugs" < mycode.py`
- **Documentation**: `@vertex "Generate README for this project"`
- **Debugging**: `@vertex "Why is this script failing?" < error.log`
- **Learning**: `@vertex "Explain how neural networks work"`

### @memory - Conversation Memory

The @memory system provides semantic search across all your AI interactions.

#### Basic Commands
```bash
# Search conversations
@memory search "database optimization"

# Recent conversations
@memory recent

# Clear memory
@memory clear

# Export conversations
@memory export conversations.json
```

#### Advanced Memory Operations
```bash
# Search with filters
@memory search "python" --days 7 --type code

# Vector similarity search
@memory similar "machine learning algorithms"

# Memory statistics
@memory stats

# Backup memory
@memory backup ~/anbs-backup/
```

#### Memory Categories
- **Commands**: All shell commands with context
- **Conversations**: AI interactions and responses
- **Files**: Analyzed files and their insights
- **Errors**: Debug sessions and solutions

### @analyze - File Analysis

The @analyze command provides AI-powered file analysis and insights.

#### Basic Analysis
```bash
# Analyze a single file
@analyze script.py

# Analyze with specific focus
@analyze --focus security database.sql

# Batch analysis
@analyze *.js --output report.md
```

#### Analysis Types
```bash
# Code quality analysis
@analyze --type quality src/

# Security vulnerability scan
@analyze --type security --recursive .

# Performance analysis
@analyze --type performance bottleneck.py

# Documentation analysis
@analyze --type docs README.md
```

#### Output Formats
```bash
# JSON output
@analyze --format json script.py > analysis.json

# Markdown report
@analyze --format markdown --output report.md src/

# Interactive analysis
@analyze --interactive complex_system.py
```

## Memory System

### How Memory Works
ANBS uses vector embeddings to create semantic understanding of your interactions. Every command, conversation, and file analysis is stored with contextual metadata.

### Vector Search
```bash
# Semantic search finds related concepts
@memory search "optimization"
# Returns: database tuning, code performance, algorithm efficiency

# Exact phrase search
@memory search '"exact phrase"'

# Time-based search
@memory search "debugging" --since "2024-01-01"
```

### Memory Categories
- **Conversations**: All @vertex interactions with full context
- **Commands**: Bash commands with success/failure status
- **Files**: Analyzed files with AI insights
- **Solutions**: Successful problem resolutions

### Memory Management
```bash
# View memory usage
@memory stats

# Clean old entries
@memory cleanup --older-than 30d

# Optimize storage
@memory optimize

# Set memory limits
@memory config --max-entries 10000
```

## File Analysis

### Supported File Types
- **Code**: .py, .js, .cpp, .java, .go, .rs, .php
- **Config**: .json, .yaml, .toml, .ini, .conf
- **Documentation**: .md, .txt, .rst, .tex
- **Data**: .csv, .xml, .sql, .log

### Analysis Features
- **Syntax Analysis**: Code structure and patterns
- **Security Scanning**: Vulnerability detection
- **Performance Insights**: Optimization opportunities
- **Best Practices**: Code quality recommendations
- **Documentation**: Auto-generated explanations

### Batch Processing
```bash
# Analyze entire project
@analyze --recursive --type all ./project/

# Parallel analysis
@analyze --parallel 4 *.py

# Generate project report
@analyze --recursive --output project-analysis.md .
```

## Health Monitoring

### System Metrics
- **CPU Usage**: Real-time processor utilization
- **Memory Usage**: RAM consumption and availability
- **Disk I/O**: Read/write operations per second
- **Network**: AI service connectivity status

### AI Agent Metrics
- **Response Time**: Average AI query processing time
- **Success Rate**: Percentage of successful AI requests
- **Queue Length**: Pending AI requests
- **Cache Hit Rate**: Percentage of cached responses

### Health Commands
```bash
# Detailed health report
@vertex --health --verbose

# Performance benchmarks
@vertex --benchmark

# System diagnostics
@vertex --diagnose
```

## Configuration

### Configuration File
ANBS configuration is stored in `~/.config/anbs/config.yaml`:

```yaml
# Display settings
display:
  split_ratio: 0.7
  theme: "dark"
  font_size: 12
  show_health_panel: true

# AI settings
ai:
  default_model: "gpt-4"
  timeout: 30
  max_context: 8192
  enable_streaming: true

# Memory settings
memory:
  max_entries: 10000
  embedding_dimension: 1536
  cleanup_interval: "24h"

# Security settings
security:
  sandbox_enabled: true
  allowed_commands: ["ls", "cat", "grep", "find"]
  max_execution_time: 300
```

### Environment Variables
```bash
# API configuration
export ANBS_API_KEY="your-api-key"
export ANBS_API_ENDPOINT="https://api.openai.com"

# Performance tuning
export ANBS_CACHE_SIZE=1000
export ANBS_THREAD_POOL_SIZE=4

# Debug settings
export ANBS_DEBUG=1
export ANBS_LOG_LEVEL=INFO
```

### Runtime Configuration
```bash
# Update settings at runtime
@vertex config set display.theme light
@vertex config set ai.timeout 60
@vertex config show
```

## Keyboard Shortcuts

### Panel Navigation
- **Tab**: Switch between terminal and AI chat panels
- **Ctrl+A**: Focus AI chat panel
- **Ctrl+T**: Focus terminal panel
- **Ctrl+H**: Focus health monitor

### Panel Management
- **Ctrl+S**: Toggle split-screen mode
- **Ctrl+M**: Maximize/minimize current panel
- **Ctrl+R**: Resize panels (then use arrow keys)
- **Ctrl+L**: Clear current panel

### AI Interaction
- **Ctrl+Space**: Quick @vertex prompt
- **Ctrl+Shift+V**: Paste last AI response
- **Ctrl+Shift+M**: Open memory search
- **Ctrl+Shift+A**: Analyze current file

### Advanced Shortcuts
- **Ctrl+Shift+H**: Health dashboard
- **Ctrl+Shift+C**: Configuration editor
- **Ctrl+Shift+D**: Debug mode toggle
- **Ctrl+Shift+Q**: Quit ANBS

## Best Practices

### Effective AI Interaction
1. **Be Specific**: "Optimize this Python function for memory usage" vs "Make this better"
2. **Provide Context**: Include relevant files, error messages, or environment details
3. **Use Memory**: Reference previous conversations with @memory search
4. **Iterate**: Build on AI responses with follow-up questions

### Memory Management
1. **Regular Cleanup**: Use `@memory cleanup` weekly
2. **Organized Searches**: Use descriptive terms and filters
3. **Export Important**: Backup valuable conversations
4. **Tag Conversations**: Add meaningful context to queries

### Performance Optimization
1. **Cache Utilization**: Repeated queries benefit from caching
2. **Batch Operations**: Analyze multiple files together
3. **Resource Monitoring**: Watch health panel for bottlenecks
4. **Configuration Tuning**: Adjust settings based on usage patterns

### Security Considerations
1. **Sandbox Mode**: Keep sandbox enabled for untrusted code
2. **Permission Review**: Regularly audit allowed commands
3. **API Key Security**: Use environment variables, never hardcode
4. **Log Monitoring**: Review debug logs for suspicious activity

## Troubleshooting

### Common Issues

#### AI Commands Not Working
```bash
# Check AI service status
@vertex --health

# Verify API configuration
echo $ANBS_API_KEY

# Test connectivity
@vertex "test" --timeout 10
```

#### Split-Screen Issues
```bash
# Reset display
Ctrl+L (in each panel)

# Restart display system
@vertex config reload display

# Check terminal size
echo $COLUMNS $LINES
```

#### Memory Problems
```bash
# Check memory usage
@memory stats

# Clear cache
@memory clear --cache-only

# Optimize database
@memory optimize
```

#### Performance Issues
```bash
# Monitor resources
@vertex --benchmark

# Reduce cache size
@vertex config set cache.size 500

# Disable features
@vertex config set ai.streaming false
```

### Debug Mode
```bash
# Enable debug logging
export ANBS_DEBUG=1

# View debug logs
tail -f ~/.config/anbs/debug.log

# Verbose health check
@vertex --health --debug
```

### Getting Help
```bash
# Command help
@vertex help
@memory help
@analyze help

# Configuration help
@vertex config help

# Online documentation
@vertex "open documentation"
```

For additional support, visit the [GitHub repository](https://github.com/user/bash-ai-native) or submit an issue with debug logs and system information.