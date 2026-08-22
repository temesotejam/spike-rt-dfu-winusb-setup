param(
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

function New-RoundedPath {
    param(
        [single]$X,
        [single]$Y,
        [single]$Width,
        [single]$Height,
        [single]$Radius
    )

    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $diameter = [single]($Radius * 2.0)

    $path.AddArc($X, $Y, $diameter, $diameter, 180, 90)
    $path.AddArc($X + $Width - $diameter, $Y, $diameter, $diameter, 270, 90)
    $path.AddArc($X + $Width - $diameter, $Y + $Height - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($X, $Y + $Height - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

function New-IconPng {
    param([int]$Size)

    $scale = [single]($Size / 256.0)
    $bitmap = [System.Drawing.Bitmap]::new(
        $Size,
        $Size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([System.Drawing.Color]::Transparent)

    $blue = [System.Drawing.ColorTranslator]::FromHtml("#1F4E79")
    $green = [System.Drawing.ColorTranslator]::FromHtml("#2A9C5B")
    $white = [System.Drawing.Color]::White

    $blueBrush = [System.Drawing.SolidBrush]::new($blue)
    $greenBrush = [System.Drawing.SolidBrush]::new($green)
    $whiteBrush = [System.Drawing.SolidBrush]::new($white)
    $whitePenOuter = [System.Drawing.Pen]::new($white, [single][Math]::Max(1.0, 14.0 * $scale))
    $whitePenUsb = [System.Drawing.Pen]::new($white, [single][Math]::Max(1.0, 12.0 * $scale))
    $whitePenCheck = [System.Drawing.Pen]::new($white, [single][Math]::Max(1.0, 10.0 * $scale))

    $whitePenOuter.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $whitePenOuter.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $whitePenUsb.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $whitePenUsb.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $whitePenUsb.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $whitePenCheck.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $whitePenCheck.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $whitePenCheck.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    try {
        $background = New-RoundedPath 0 0 ([single]$Size) ([single]$Size) ([single](52 * $scale))
        $graphics.FillPath($blueBrush, $background)
        $background.Dispose()

        $inner = New-RoundedPath `
            ([single](42 * $scale)) `
            ([single](42 * $scale)) `
            ([single](172 * $scale)) `
            ([single](172 * $scale)) `
            ([single](28 * $scale))
        $graphics.DrawPath($whitePenOuter, $inner)
        $inner.Dispose()

        $graphics.DrawLine(
            $whitePenUsb,
            [single](128 * $scale), [single](82 * $scale),
            [single](128 * $scale), [single](162 * $scale)
        )
        $graphics.DrawLine(
            $whitePenUsb,
            [single](128 * $scale), [single](112 * $scale),
            [single](94 * $scale), [single](92 * $scale)
        )
        $graphics.DrawLine(
            $whitePenUsb,
            [single](128 * $scale), [single](128 * $scale),
            [single](166 * $scale), [single](104 * $scale)
        )

        $triangle = [System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new([single](128 * $scale), [single](68 * $scale)),
            [System.Drawing.PointF]::new([single](114 * $scale), [single](90 * $scale)),
            [System.Drawing.PointF]::new([single](142 * $scale), [single](90 * $scale))
        )
        $graphics.FillPolygon($whiteBrush, $triangle)

        $graphics.FillRectangle(
            $whiteBrush,
            [single](79 * $scale), [single](77 * $scale),
            [single](18 * $scale), [single](18 * $scale)
        )
        $graphics.FillEllipse(
            $whiteBrush,
            [single](160 * $scale), [single](90 * $scale),
            [single](20 * $scale), [single](20 * $scale)
        )

        $graphics.FillEllipse(
            $greenBrush,
            [single](141 * $scale), [single](142 * $scale),
            [single](78 * $scale), [single](78 * $scale)
        )
        $graphics.DrawEllipse(
            $whitePenOuter,
            [single](141 * $scale), [single](142 * $scale),
            [single](78 * $scale), [single](78 * $scale)
        )
        $graphics.DrawLine(
            $whitePenCheck,
            [single](161 * $scale), [single](181 * $scale),
            [single](176 * $scale), [single](196 * $scale)
        )
        $graphics.DrawLine(
            $whitePenCheck,
            [single](176 * $scale), [single](196 * $scale),
            [single](202 * $scale), [single](165 * $scale)
        )

        $stream = [System.IO.MemoryStream]::new()
        try {
            $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            return ,$stream.ToArray()
        }
        finally {
            $stream.Dispose()
        }
    }
    finally {
        $whitePenCheck.Dispose()
        $whitePenUsb.Dispose()
        $whitePenOuter.Dispose()
        $whiteBrush.Dispose()
        $greenBrush.Dispose()
        $blueBrush.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = [System.Collections.Generic.List[byte[]]]::new()
foreach ($size in $sizes) {
    $images.Add((New-IconPng -Size $size))
}

$fullOutput = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = [System.IO.Path]::GetDirectoryName($fullOutput)
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$file = [System.IO.File]::Open(
    $fullOutput,
    [System.IO.FileMode]::Create,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::None
)
$writer = [System.IO.BinaryWriter]::new($file)

try {
    $writer.Write([UInt16]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]$images.Count)

    $offset = 6 + (16 * $images.Count)

    for ($i = 0; $i -lt $images.Count; $i++) {
        $size = $sizes[$i]
        $bytes = $images[$i]
        $encodedSize = if ($size -eq 256) { 0 } else { $size }

        $writer.Write([byte]$encodedSize)
        $writer.Write([byte]$encodedSize)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]$bytes.Length)
        $writer.Write([UInt32]$offset)

        $offset += $bytes.Length
    }

    foreach ($bytes in $images) {
        $writer.Write($bytes)
    }
}
finally {
    $writer.Dispose()
    $file.Dispose()
}

Write-Host "Generated PNG-frame ICO: $fullOutput"
