from PIL import Image, ImageDraw
import os

def create_pwa_icon(size, filename, output_dir):
    # Modern Gradient Colors
    # Blue-ish to Red-ish vibrant gradient
    color_start = (99, 179, 237, 255) # #63b3ed
    color_end = (252, 129, 129, 255)   # #fc8181
    
    # Create image with transparent background
    img = Image.new('RGBA', (size, size), color=(0,0,0,0))
    
    center = size // 2
    
    # Define lightning bolt points (Zap)
    # Scaled to fill more space since there's no background circle
    z = size // 2.5
    zap_points = [
        (center + z//2.5, center - z),
        (center - z//1.2, center + z//6),
        (center - z//8,   center + z//6),
        (center - z//2.5, center + z),
        (center + z//1.2, center - z//6),
        (center + z//8,   center - z//6)
    ]
    
    # Mask for the zap shape
    mask = Image.new('L', (size, size), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.polygon(zap_points, fill=255)
    
    # Create the modern gradient
    gradient = Image.new('RGBA', (size, size), (0,0,0,0))
    for y in range(size):
        # Diagonal-ish gradient factor
        for x in range(size):
            # Calculate ratio based on diagonal (top-left to bottom-right)
            ratio = (x + y) / (2 * size)
            ratio = max(0, min(1, ratio))
            
            r = int(color_start[0] + (color_end[0] - color_start[0]) * ratio)
            g = int(color_start[1] + (color_end[1] - color_start[1]) * ratio)
            b = int(color_start[2] + (color_end[2] - color_start[2]) * ratio)
            
            # Using point draw for per-pixel gradient (slow but accurate for small icons)
            gradient.putpixel((x, y), (r, g, b, 255))
    
    # Composite zap with gradient onto main image
    img.paste(gradient, (0, 0), mask=mask)
    
    # For .ico and Apple Touch, we usually need a solid background or it might look weird
    # However, let's stick to the user's "just the lightning" request.
    # Note: Android adaptive icons might clip this if not padded.
    
    # Save
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    path = os.path.join(output_dir, filename)
    
    if filename.endswith(".ico"):
        # Favicons usually need a background for visibility in some browsers
        # or just stay transparent. Let's keep it transparent.
        img.save(path)
    else:
        img.save(path)
    print(f"Created: {path}")

# Generate all needed sizes
web_public = "web/public"
create_pwa_icon(192, "pwa-192x192.png", web_public)
create_pwa_icon(512, "pwa-512x512.png", web_public)
create_pwa_icon(180, "apple-touch-icon.png", web_public)
create_pwa_icon(32, "favicon.ico", web_public)
