import { useGame } from '../game/state';
import { MenuScreen } from './screens/MenuScreen';
import { HubScreen } from './screens/HubScreen';
import { CharacterScreen } from './screens/CharacterScreen';
import { InventoryScreen } from './screens/InventoryScreen';
import { ShopScreen } from './screens/ShopScreen';
import { GarageScreen } from './screens/GarageScreen';
import { TrainingScreen } from './screens/TrainingScreen';
import { FenceScreen } from './screens/FenceScreen';
import { InformantScreen } from './screens/InformantScreen';
import { HeistsScreen } from './screens/HeistsScreen';
import { PlanScreen } from './screens/PlanScreen';
import { ExecScreen } from './screens/ExecScreen';
import { DebriefScreen } from './screens/DebriefScreen';

export function App() {
  const screen = useGame((s) => s.screen);
  const started = useGame((s) => s.started);

  // Ohne Spielstand immer ins Menü
  if (!started && screen !== 'menu') {
    return <MenuScreen />;
  }

  switch (screen) {
    case 'menu':
      return <MenuScreen />;
    case 'hub':
      return <HubScreen />;
    case 'character':
      return <CharacterScreen />;
    case 'inventory':
      return <InventoryScreen />;
    case 'shop':
      return <ShopScreen />;
    case 'garage':
      return <GarageScreen />;
    case 'training':
      return <TrainingScreen />;
    case 'fence':
      return <FenceScreen />;
    case 'informant':
      return <InformantScreen />;
    case 'heists':
      return <HeistsScreen />;
    case 'plan':
      return <PlanScreen />;
    case 'exec':
      return <ExecScreen />;
    case 'debrief':
      return <DebriefScreen />;
    default:
      return <MenuScreen />;
  }
}
