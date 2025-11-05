### Initialize players as AI or human

In `program/src/GameState.cpp`, in find `InitializePlayerControllers()` around line 1990.

**Make Mr. X AI-controlled:**
```cpp
void GameState::InitializePlayerControllers() {
    m_vec_PlayerControllers.clear();

    for (size_t i = 0; i < m_vec_Players.size(); ++i) {
        if (m_vec_Players[i].GetType() == Core::PlayerType::MisterX) {
            // AI with 1.5 second turn delay
            // currently in AI function the algorythm chooses random move
            // you can add jumps to your algorythms there
            // the best way is to add enum for algorythms and pass it here ↓ ↓ ↓
            m_vec_PlayerControllers.push_back(
                std::make_unique<Core::AIPlayerController>(1.5f) // (1.5f, enum)
            );
        } else {
            // Detectives
            m_vec_PlayerControllers.push_back(
                std::make_unique<Core::HumanPlayerController>()
            );
        }
    }
}
```

### Add Algorithm

In `program/src/PlayerController.cpp` in `CalculateBestMove()` around line 70.

Replace the placeholder random algorithm with your own:

```cpp
MoveDecision AIPlayerController::CalculateBestMove(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    MoveDecision decision;

    //pobranie info zależnie kto pyta

    //pewnie jakiś switch(enum) i funkcje z algorytmami

    return decision;
}
```

### Thread Safety for AI Algorithms

**IMPORTANT**: AI calculations run on background threads to prevent UI freezing.

**Thread-Safety Rules:**
1. **Never use static variables without mutex protection** - They're shared across all AI threads
2. **Don't use `p_Player` pointer** - It may become invalid. Use `gameState.vec_AllPlayers[gameState.i_CurrentPlayerIndex]` instead
3. **Capture by value in lambdas** - Use `[this, vec_PossibleMoves, gameState]` not `[&]`

**Example: Thread-safe static cache**
```cpp
static std::map<int, int> s_Cache;
static std::mutex s_CacheMutex;

// Check cache
{
    std::lock_guard<std::mutex> lock(s_CacheMutex);
    if (s_Cache.count(key)) return s_Cache[key];
}

// Compute value...

// Store in cache
{
    std::lock_guard<std::mutex> lock(s_CacheMutex);
    s_Cache[key] = value;
}
```

---

## Available Game State Data

List of information you can fetch

### Game Info
```cpp
gameState.i_CurrentPlayerIndex  // Which player is making this move (0-4)
gameState.i_CurrentRound        // Current game round (1-24)
gameState.b_IsRevealRound       // True if Mr. X is revealed this round
gameState.b_IsNextRevealRound   // True if Mr. X reveals after this move.
```

### Player Information
```cpp
// gameState.vec_AllPlayers[i] contains:
struct PlayerInfo {
    int i_Position;           // Current node (1-200)
    bool b_IsVisible;         // Is position visible to current player?
    bool b_IsMisterX;         // True if this is Mr. X

    // Ticket counts
    int i_TaxiTickets;        // detectives
    int i_BusTickets;         // detectives
    int i_MetroTickets;       // detectives
    int i_BlackTickets;       // Mr. X
    int i_DoubleMoveTickets;  // Mr. X
};
```

### Graph Access
```cpp
// Get connections from a node
auto connections = gameState.p_Graph->GetConnections(nodeId);
for (const auto& conn : connections) {
    int destNode = conn.i_NodeId;
    int transport = conn.i_TransportType;  // k_TransportTypeTaxi, Bus, Metro, Water
}

// Get neighbors by specific transport type
auto neighbors = gameState.p_Graph->GetNeighborsByType(nodeId, Core::k_TransportTypeTaxi);

// Check if node is valid
bool valid = gameState.p_Graph->IsValidNode(nodeId);
```

### Constants
Located in `GameConstants.h`:
```cpp
Core::k_TransportTypeTaxi    // 1
Core::k_TransportTypeBus     // 2
Core::k_TransportTypeMetro   // 3
Core::k_TransportTypeWater   // 4

Core::k_RevealRounds[]  // {3, 8, 13, 18, 24}
Core::IsRevealRound(round)  // Check if a round is a reveal round
```