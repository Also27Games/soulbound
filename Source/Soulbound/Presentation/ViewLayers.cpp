#include "ViewLayers.h"

int32 Soulbound::Presentation::ViewLayerZOrder(EViewLayer Layer)
{
	// Las distancias de antes (del 9 al 14): la carta del Elegido, el aviso y el anillo comparten la suya.
	switch (Layer)
	{
	case EViewLayer::Dim:
		return ViewLayerBase;
	case EViewLayer::Status:
		return ViewLayerBase + 1;
	case EViewLayer::WardenLine:
		return ViewLayerBase + 2;
	case EViewLayer::ReturnToMenu:
		return ViewLayerBase + 3;
	case EViewLayer::ElectCard:
	case EViewLayer::Notice:
	case EViewLayer::Ring:
		return ViewLayerBase + 4;
	case EViewLayer::Grimoire:
		return ViewLayerBase + 5;
	}
	return ViewLayerBase;
}
