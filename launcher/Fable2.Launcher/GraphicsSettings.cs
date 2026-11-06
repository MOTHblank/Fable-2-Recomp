using System.Globalization;
using Fable2Launcher.Constants;

namespace Fable2Launcher;

public static class GraphicsSettings
{
    public const int MaxInternalRenderScale = 7;
    public const int MinOutputWidth = 640;
    public const int MinOutputHeight = 360;
    public const int MaxOutputDimension = 16384;
    public const double MinSupportedAspect = 4.0 / 3.0;
    public const double MaxSupportedAspect = 4.0;
    public static readonly string[] ManagedKeys =
    [
        "resolution", "window_width", "window_height", "resolution_scale",
        "anisotropic_override", "swap_post_effect", "vsync", "fullscreen", "fullscreen_exclusive",
        "frame_limit", "guest_vblank_pacing", "fast_guest_vblank_hz", "launcher_renderer"
    ];

    // A launcher-only preference, not a ReXGlue SDK cvar.
    public static readonly string[] SdkManagedKeys =
        ManagedKeys.Where(key => key != "launcher_renderer").ToArray();

    public static bool TryResolveOutputResolution(string resolution, string customWidth,
        string customHeight, out int width, out int height)
    {
        (width, height) = resolution switch
        {
            "720p" => (1280, 720),
            "1080p" => (1920, 1080),
            "1440p" => (2560, 1440),
            "4k" => (3840, 2160),
            _ => (0, 0)
        };
        if (resolution == "custom")
        {
            if (!int.TryParse(customWidth, NumberStyles.Integer, CultureInfo.InvariantCulture,
                    out width) ||
                !int.TryParse(customHeight, NumberStyles.Integer, CultureInfo.InvariantCulture,
                    out height))
                return false;
        }
        else if (width == 0 || height == 0)
        {
            return false;
        }

        if (width < MinOutputWidth || height < MinOutputHeight ||
            width > MaxOutputDimension || height > MaxOutputDimension)
            return false;

        double aspect = (double)width / height;
        return aspect >= MinSupportedAspect && aspect <= MaxSupportedAspect;
    }

    public static Dictionary<string, string> Create(string resolution, string scale,
        string anisotropic, string antiAliasing, string displayMode, bool vsync,
        string frameLimit = "0", string backend = "d3d12",
        string customWidth = "1920", string customHeight = "1080")
    {
        if (!TryResolveOutputResolution(resolution, customWidth, customHeight,
                out int width, out int height))
            throw new ArgumentException(
                $"Unsupported output size. Use {MinOutputWidth}×{MinOutputHeight} through " +
                $"{MaxOutputDimension}×{MaxOutputDimension} with an aspect ratio between " +
                "4:3 and 4:1.");
        bool validScale = int.TryParse(scale, NumberStyles.Integer, CultureInfo.InvariantCulture,
            out int scaleValue) && scaleValue >= 1 && scaleValue <= MaxInternalRenderScale;
        if (!validScale ||
            !new[] { "-1", "1", "2", "3", "4", "5" }.Contains(anisotropic) ||
            !new[] { "none", "fxaa", "fxaa_extreme" }.Contains(antiAliasing) ||
            !new[] { "windowed", "borderless", "exclusive" }.Contains(displayMode) ||
            !new[] { "d3d12", "vulkan" }.Contains(backend) ||
            !GraphicsOptions.FrameLimits.Any(option => option.Value == frameLimit))
            throw new ArgumentException("Unsupported graphics setting");

        return new(StringComparer.OrdinalIgnoreCase)
        {
            // Presentation size is independent from the guest's original 720p mode.
            ["resolution"] = "\"\"",
            ["window_width"] = width.ToString(CultureInfo.InvariantCulture),
            ["window_height"] = height.ToString(CultureInfo.InvariantCulture),
            ["resolution_scale"] = scale,
            ["anisotropic_override"] = anisotropic,
            ["swap_post_effect"] = $"\"{antiAliasing}\"",
            ["vsync"] = vsync.ToString().ToLowerInvariant(),
            ["frame_limit"] = frameLimit,
            // Fable waits for two guest vblanks per frame. Legacy 60-Hz
            // guest pacing therefore caps it at 30 even with a 60-FPS limiter.
            // This does not disable the user's host presentation VSync.
            ["guest_vblank_pacing"] = (frameLimit == "30").ToString().ToLowerInvariant(),
            // Fable waits for two guest vblanks per frame. A 3x margin above
            // that minimum reduces interrupt overhead without becoming the
            // 60-FPS gate. Legacy 30 and unlimited retain the old 1000 Hz.
            ["fast_guest_vblank_hz"] = frameLimit is "0" or "30" ? "1000" :
                Math.Min(1000, int.Parse(frameLimit, CultureInfo.InvariantCulture) * 6)
                    .ToString(CultureInfo.InvariantCulture),
            ["fullscreen"] = (displayMode != "windowed").ToString().ToLowerInvariant(),
            ["fullscreen_exclusive"] = (displayMode == "exclusive").ToString().ToLowerInvariant(),
            ["launcher_renderer"] = $"\"{backend}\""
        };
    }
}
