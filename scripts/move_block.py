import sys

filepath = 'web/src/components/tabs/SnifferTab.tsx'
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

start_marker = '<div className="bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl flex flex-col min-h-[400px]">'
analysis_start = content.find(start_marker)

if analysis_start == -1:
    print('Analysis block not found')
    sys.exit(1)

# Find the div closing just before PacketStream
grid_end = content.find('</div>\n\n        <PacketStream', analysis_start)

if grid_end == -1:
    print('Grid end not found')
    sys.exit(1)

analysis_block = content[analysis_start:grid_end]

# Extract the block
new_content = content[:analysis_start] + content[grid_end:]

# Find the end of the PacketStream block
ps_end_marker = 'onFilter={setFilterProto}\n        />'
ps_end = new_content.find(ps_end_marker)

if ps_end == -1:
    print('Packet stream end not found')
    sys.exit(1)

ps_end += len(ps_end_marker)

insertion = '\n\n        <div className="grid grid-cols-1 gap-4 mx-4 mb-4">\n          ' + analysis_block.replace('\n', '\n          ') + '\n        </div>'

# The analysis_block already has its own indentation, let's preserve it but wrap it inside <div className="mx-4 mb-4">
# We can just use the original block, but wrapped in a grid col to match the previous container
insertion = '\n\n        <div className="grid grid-cols-1 gap-4 mx-4 mb-4">\n' + analysis_block + '\n        </div>\n'

final_content = new_content[:ps_end] + insertion + new_content[ps_end:]

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(final_content)

print('Successfully moved the block.')
