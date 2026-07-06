# Generate placeholder icon PNGs for VPK packaging
# Uses .NET to create minimal valid PNG files

Add-Type -AssemblyName System.Drawing

function New-PlaceholderIcon($path, $width, $height, $bgColor, $text) {
    $bmp = New-Object System.Drawing.Bitmap($width, $height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear($bgColor)
    
    $font = New-Object System.Drawing.Font("Arial", 12, [System.Drawing.FontStyle]::Bold)
    $brush = [System.Drawing.Brushes]::White
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = [System.Drawing.StringAlignment]::Center
    $fmt.LineAlignment = [System.Drawing.StringAlignment]::Center
    
    $rect = New-Object System.Drawing.RectangleF(0, 0, $width, $height)
    $g.DrawString($text, $font, $brush, $rect, $fmt)
    
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    Write-Output "Created: $path"
}

$base = Split-Path -Parent $MyInvocation.MyCommand.Path

# App icon (128x128)
New-PlaceholderIcon "$base\sce_sys\icon0.png" 128 128 ([System.Drawing.Color]::FromArgb(64, 80, 160)) "JME"

# LiveArea background (840x500)
New-PlaceholderIcon "$base\sce_sys\livearea\background\bg.png" 840 500 ([System.Drawing.Color]::FromArgb(50, 50, 80)) "Java ME Emulator"

# Startup image (280x158) 
New-PlaceholderIcon "$base\sce_sys\startup.png" 280 158 ([System.Drawing.Color]::FromArgb(64, 80, 160)) "JME"

Write-Output "Done. Placeholder icons created."
