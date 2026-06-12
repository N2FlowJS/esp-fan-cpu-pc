from PIL import Image, ImageDraw, ImageFont
import os

def create_pwa_icon(size, filename, output_dir):
    # Colors matching UI
    bg_color = (10, 12, 18) # --bg: #0a0c12
    accent_color = (99, 179, 237) # --accent: #63b3ed
    
    # Create image with background
    img = Image.new('RGB', (size, size), color=bg_color)
    draw = ImageDraw.Draw(img)
    
    # Draw a stylized hex/circle border
    padding = size // 10
    draw.ellipse([padding, padding, size-padding, size-padding], outline=accent_color, width=size//40)
    
    # Draw a stylized "Bolt" or "Fan" shape
    # Simple Bolt shape coordinates
    center = size // 2
    s = size // 4
    bolt_points = [
        (center + s//2, center - s),
        (center - s, center + s//4),
        (center, center + s//4),
        (center - s//2, center + s),
        (center + s, center - s//4),
        (center, center - s//4)
    ]
    draw.polygon(bolt_points, fill=accent_color)
    
    # Save
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    path = os.path.join(output_dir, filename)
    img.save(path)
    print(f"Created: {path}")

# Generate all needed sizes
web_public = "web/public"
create_pwa_icon(192, "pwa-192x192.png", web_public)
create_pwa_icon(512, "pwa-512x512.png", web_public)
create_pwa_icon(180, "apple-touch-icon.png", web_public)
create_pwa_icon(32, "favicon.ico", web_public)
