# Dice Bluffing Game

Welcome to the Dice Bluffing Game! This project demonstrates a multiplayer dice-based strategy game using a client-server architecture. Follow the steps below to set up, compile, and run the game.

## Prerequisites

Before proceeding, ensure you have the **unpv13e environment** set up as described in the class instructions. For guidance on installing and setting up unpv13e, refer to the following link:

[unpv13e Installation Instructions](https://www.cs.nycu.edu.tw/~lhyen/np/unpv13e_install.html)

## Repository Structure

```
Repository
│   README.md
│
└───unpv13e
    │
    └───tcpcliserv
        │   server.c
        │   client.c
        │   Makefile
```

### Setup Instructions

1. **Prepare the Environment**:
   - Ensure the `server.c`, `client.c`, and `makefile` are located in the `unpv13e/tcpcliserv/` directory.

2. **Compile the Code**:
   - Open a terminal and navigate to the `tcpcliserv` directory.
   - Run the following command to compile the server and client code:
     ```
     make
     ```

3. **Run the Game**:
   - Start the server by executing:
     ```
     ./server
     ```
   - Connect a client to the server by executing:
     ```
     ./client <server_ip_address>
     ```
     Replace `<server_ip_address>` with the IP address of the server.

4. **Enjoy the Game**:
   - Once the client connects, you can enjoy playing the Dice Bluffing Game!

## Game Overview

In the Dice Bluffing Game, players take turns bidding, calling, or spotting dice values based on strategy and bluffing. The goal is to outwit other players and be the last one standing.

### Features
- **Multiplayer Gameplay**: Supports multiple players over a network.
- **Customizable Settings**: The first player can configure dice count and life count.
- **Timeout Mechanism**: Ensures timely gameplay with default actions.
- **Robust Networking**: Handles client disconnections gracefully.

## Additional Information

Refer to the project report for detailed insights into the game design, research methods, and future improvements.

## Credits

- **Team Members**:
  - 111550004 翁世芳
  - 111550137 林宏儒
  - 112550083 張亦陞