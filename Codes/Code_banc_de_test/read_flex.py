import serial
import time
import matplotlib.pyplot as plt
import csv

# --- CONFIGURATION ---
SERIAL_PORT = "COM3"
BAUD_RATE = 115200

arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
time.sleep(2)

print("Connexion établie avec Arduino.")
print("En attente du début du cycle...")

# --- DONNÉES ---
angles_aller, angles_retour = [], []

flex_aller, flex_retour = [], []
graphite_aller, graphite_retour = [], []

mode_detected = None  # "flex", "graphite", "both"

cycle_started = False

try:
    while True:
        line = arduino.readline().decode().strip()

        if not line:
            continue

        # DEBUG (tu peux commenter après)
        print("RAW:", line)

        if line == "START":
            cycle_started = True
            print("Cycle démarré...")
            continue

        elif line == "END":
            print("Cycle terminé.")
            break

        if not cycle_started:
            continue

        # --- Parsing ---
        if line.startswith("A") or line.startswith("R"):
            phase = line[0]
            parts = line[1:].split("\t")

            # Sécurité
            if len(parts) < 2:
                continue

            angle = float(parts[0])

            if phase == "A":
                angles_aller.append(angle)
            else:
                angles_retour.append(angle)

            # --- CAS 1 : UN SEUL CAPTEUR ---
            if len(parts) == 2:
                value = float(parts[1])

                # 🔥 Détection intelligente selon amplitude
                if value > 10:  # typiquement graphite (~500 ADC)
                    if phase == "A":
                        graphite_aller.append(value)
                    else:
                        graphite_retour.append(value)
                else:  # typiquement flex (ou faible signal)
                    if phase == "A":
                        flex_aller.append(value)
                    else:
                        flex_retour.append(value)

            # --- CAS 2 : DEUX CAPTEURS ---
            elif len(parts) == 3:
                flex_val = float(parts[1])
                graphite_val = float(parts[2])

                if phase == "A":
                    flex_aller.append(flex_val)
                    graphite_aller.append(graphite_val)
                else:
                    flex_retour.append(flex_val)
                    graphite_retour.append(graphite_val)


except KeyboardInterrupt:
    arduino.write("STOP\n".encode())
    print("Cycle interrompu.")

arduino.close()
print("Données récupérées.")

# --- AFFICHAGE ---
plt.figure(figsize=(8,6))

# Cas 1 : un seul capteur
if len(graphite_aller) == 0:
    plt.scatter(angles_aller, flex_aller, s=10, color='blue', label='Aller')
    plt.scatter(angles_retour, flex_retour, s=10, color='navy', label='Retour')
    plt.xlabel("Angle (°)")
    plt.ylabel("Signal capteur")
    plt.title("Mesure capteur")
    plt.legend()
    plt.grid(True)

# Cas 2 : deux capteurs
else:
    fig, ax1 = plt.subplots(figsize=(8,6))

    # --- FLEX (axe gauche) ---
    ax1.scatter(angles_aller, flex_aller, s=10, color='blue', label='Flex Aller')
    ax1.scatter(angles_retour, flex_retour, s=10, color='navy', label='Flex Retour')
    ax1.set_xlabel("Angle (°)")
    ax1.set_ylabel("Flex (Ω)")
    ax1.grid(True)

    # --- GRAPHITE (axe droit) ---
    ax2 = ax1.twinx()
    ax2.scatter(angles_aller, graphite_aller, s=10, color='red', marker='x', label='Graphite Aller')
    ax2.scatter(angles_retour, graphite_retour, s=10, color='salmon', marker='x', label='Graphite Retour')
    ax2.set_ylabel("Graphite (signal amplifié)")

    # --- Légende combinée ---
    lines_1, labels_1 = ax1.get_legend_handles_labels()
    lines_2, labels_2 = ax2.get_legend_handles_labels()
    ax1.legend(lines_1 + lines_2, labels_1 + labels_2, loc='best')

    plt.title("Comparaison Flex vs Graphite")

plt.show()
# --- SAUVEGARDE CSV ---
save = input("Enregistrer les données CSV ? (y/n) : ")

if save.lower() == "y":
    with open("flex_graphite_data.csv", "w", newline="") as f:
        writer = csv.writer(f)

        if len(graphite_aller) == 0:
            writer.writerow(["Phase","Angle","Valeur"])
            for a,v in zip(angles_aller, flex_aller):
                writer.writerow(["Aller", a, v])
            for a,v in zip(angles_retour, flex_retour):
                writer.writerow(["Retour", a, v])
        else:
            writer.writerow(["Phase","Angle","Flex","Graphite"])
            for a,fv,gv in zip(angles_aller, flex_aller, graphite_aller):
                writer.writerow(["Aller", a, fv, gv])
            for a,fv,gv in zip(angles_retour, flex_retour, graphite_retour):
                writer.writerow(["Retour", a, fv, gv])

    print("CSV enregistré : flex_graphite_data.csv")