#pragma once

#include "Core/CommonTypes.h"

#include <afxwin.h>

namespace ElevatorStatusPalette
{
	inline constexpr COLORREF MovingUpFill = RGB(37, 99, 235);
	inline constexpr COLORREF MovingUpBorder = RGB(29, 78, 216);
	inline constexpr COLORREF MovingDownFill = RGB(234, 88, 12);
	inline constexpr COLORREF MovingDownBorder = RGB(194, 65, 12);
	inline constexpr COLORREF ServicingFill = RGB(124, 58, 237);
	inline constexpr COLORREF ServicingBorder = RGB(109, 40, 217);
	inline constexpr COLORREF FullFill = RGB(220, 38, 38);
	inline constexpr COLORREF FullBorder = RGB(153, 27, 27);
	inline constexpr COLORREF IdleFill = RGB(203, 213, 225);
	inline constexpr COLORREF IdleBorder = RGB(100, 116, 139);
	inline constexpr COLORREF LightText = RGB(255, 255, 255);
	inline constexpr COLORREF DarkText = RGB(30, 41, 59);

	struct Colors
	{
		COLORREF fill;
		COLORREF border;
		COLORREF text;
	};

	inline Colors Resolve(const ElevatorSnapshot& elevator) noexcept
	{
		if (elevator.capacity > 0 && elevator.passengerCount >= elevator.capacity)
			return { FullFill, FullBorder, LightText };

		switch (elevator.state)
		{
		case ElevatorState::MovingUp:
			return { MovingUpFill, MovingUpBorder, LightText };
		case ElevatorState::MovingDown:
			return { MovingDownFill, MovingDownBorder, LightText };
		case ElevatorState::Boarding:
		case ElevatorState::Alighting:
		case ElevatorState::Stopped:
			return { ServicingFill, ServicingBorder, LightText };
		case ElevatorState::Idle:
		default:
			return { IdleFill, IdleBorder, DarkText };
		}
	}
}
