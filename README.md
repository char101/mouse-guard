This application detects the mouse movement when it is in the right edge of the
primary monitor. If the mouse position crosses it slowly (i.e. the different
between last mouse horizontal position and current horizotal position is less
than 5), then the mouse is moved back to the primary monitor. This prevents the
mouse from crossing the monitor when dragging scrollbars at the right edge.

When the mouse is moved quickly between monitors, the horizontal position
movement will be greater than 5 so it will not be blocked.
