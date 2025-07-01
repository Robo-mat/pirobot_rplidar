import pygame
import math
import sys
from pathlib import Path

# --- Configurazione Pygame ---
WIDTH, HEIGHT = 800, 800  # Dimensioni della finestra di Pygame
CENTER_X, CENTER_Y = WIDTH // 2, HEIGHT // 2 # Centro dello schermo
BACKGROUND_COLOR = (0, 0, 0) # Nero
POINT_COLOR = (0, 255, 0)    # Verde (per i punti del LiDAR)
AXIS_COLOR = (50, 50, 50)    # Grigio scuro per gli assi
GRID_COLOR = (20, 20, 20)    # Grigio molto scuro per la griglia

# --- Scala di Visualizzazione ---
# Questo è il valore massimo di distanza (in millimetri) che il visualizzatore tenterà di mostrare.
# Se la distanza massima nel tuo file è inferiore a questo, la scala verrà comunque basata su questo valore,
# per evitare un ingrandimento eccessivo. Impostalo in base al range massimo del tuo LiDAR (es. 5 metri = 5000 mm).
FIXED_MAX_DISTANCE_MM = 10000.0 # Esempio: 5 metri

# --- Funzione per leggere e parsificare il file ---
def load_lidar_data(filepath):
    points = [] # Lista per salvare (angolo, distanza)
    max_distance = 0.0 # Per tenere traccia della distanza massima effettiva nel file
    try:
        with open(filepath, 'r') as f:
            for line in f:
                # Rimuovi spazi extra e poi dividi per " "
                parts = line.strip().split()
                
                try:
                    theta = float(parts[0])
                    distance = float(parts[1])
                except (ValueError, IndexError) as e:
                    print(f"Errore di parsificazione riga '{line.strip()}': {e}")
                    continue # Salta la riga e continua
                
                if distance > 0.0: # Ignora distanze non valide o zero
                    points.append((theta, distance))
                    if distance > max_distance:
                        max_distance = distance # Aggiorna la distanza massima

                
    except FileNotFoundError:
        print(f"Errore: File '{filepath}' non trovato.")
        sys.exit(1) # Esci dal programma
    
    return points, max_distance # Restituisce anche la distanza massima

# --- Funzione per convertire coordinate polari in cartesiane ---
def polar_to_cartesian(angle_degrees, distance_mm, scale):
    # La maggior parte dei LiDAR ha 0 gradi in avanti (asse Y+) e angoli che crescono in senso orario.
    # Il sistema Pygame (e matematico standard) ha 0 gradi sull'asse X+ e angoli che crescono in senso antiorario.
    
    # Per far sì che 0 gradi del LiDAR sia in alto (asse Y positivo) e gli angoli aumentino in senso orario,
    # dobbiamo convertire l'angolo del LiDAR in un angolo compatibile con la matematica standard (0=destra, antiorario).
    # L'angolo 0 del LiDAR è 90 gradi in Pygame standard.
    # L'angolo 90 del LiDAR è 0 gradi in Pygame standard.
    # L'angolo 180 del LiDAR è -90 gradi in Pygame standard.
    # L'angolo 270 del LiDAR è -180 gradi in Pygame standard.
    
    # La formula di conversione è (90 - angle_degrees_lidar) per allineare 0 LiDAR con Y+ e avere crescita oraria.
    # `adjusted_angle_degrees = (90 - angle_degrees)`
    
    # Converti l'angolo ajustato da gradi a radianti
    # Usiamo 90 - angle_degrees per ruotare l'asse di riferimento.
    adjusted_angle_radians = math.radians(90 - angle_degrees)

    # Calcola le coordinate cartesiane relative al centro del LiDAR
    # Ora x e y sono calcolate in base all'orientamento desiderato (0 in alto)
    x = distance_mm * math.cos(adjusted_angle_radians) * scale
    y = distance_mm * math.sin(adjusted_angle_radians) * scale
    
    # Pygame ha l'asse Y che cresce verso il basso. Per una visualizzazione standard
    # (Y in alto), invertiamo il valore di y calcolato.
    y = -y 

    # Trasla il punto per avere il centro LiDAR al centro dello schermo Pygame
    display_x = int(x + CENTER_X)
    display_y = int(y + CENTER_Y)

    return display_x, display_y

# --- Funzione per disegnare la griglia e gli assi ---
def draw_grid_and_axes(screen, effective_max_distance_mm, scale_factor):
    # Raggio massimo in pixel che possiamo disegnare nella finestra (metà della dimensione più piccola)
    max_screen_radius_pixels = min(WIDTH, HEIGHT) // 2
    
    # Decidiamo un intervallo per le etichette della griglia, ad esempio ogni 1 metro (1000 mm)
    grid_interval_mm = 1000 
    
    # Se la distanza massima effettiva è molto piccola, potremmo voler un intervallo più piccolo per la griglia
    if effective_max_distance_mm < 1000:
        grid_interval_mm = 250 # Ogni 250 mm se le distanze sono piccole
    
    # Disegna cerchi concentrici per la griglia fino alla distanza massima effettiva più un piccolo margine
    for r_mm in range(grid_interval_mm, int(effective_max_distance_mm * 1.1) + grid_interval_mm, grid_interval_mm): 
        radius_pixels = int(r_mm * scale_factor)
        if radius_pixels < max_screen_radius_pixels and radius_pixels > 0:
            pygame.draw.circle(screen, GRID_COLOR, (CENTER_X, CENTER_Y), radius_pixels, 1)
            
            # Etichetta i cerchi con la distanza in metri
            font = pygame.font.Font(None, 15)
            text_surface = font.render(f"{r_mm/1000:.1f}m", True, GRID_COLOR)
            text_rect = text_surface.get_rect(center=(CENTER_X + radius_pixels + 15, CENTER_Y))
            screen.blit(text_surface, text_rect)

    # Assi (Linee)
    # L'asse Y (che ora rappresenta 0 gradi del LiDAR)
    pygame.draw.line(screen, AXIS_COLOR, (CENTER_X, 0), (CENTER_X, HEIGHT), 1) 
    # L'asse X (che rappresenta 90 e 270 gradi del LiDAR)
    pygame.draw.line(screen, AXIS_COLOR, (0, CENTER_Y), (WIDTH, CENTER_Y), 1)

    # Etichette assi (Aggiornate per il nuovo orientamento)
    font = pygame.font.Font(None, 20)
    screen.blit(font.render("0° (+Y)", True, AXIS_COLOR), (CENTER_X + 5, 5)) # Ora 0 gradi è in alto
    screen.blit(font.render("90° (+X)", True, AXIS_COLOR), (WIDTH - 40, CENTER_Y - 20)) # 90 gradi è a destra
    screen.blit(font.render("180° (-Y)", True, AXIS_COLOR), (CENTER_X + 5, HEIGHT - 25)) # 180 gradi è in basso
    screen.blit(font.render("270° (-X)", True, AXIS_COLOR), (5, CENTER_Y - 20)) # 270 gradi è a sinistra


# --- Funzione principale ---
def main():
    pygame.init() # Inizializza Pygame
    screen = pygame.display.set_mode((WIDTH, HEIGHT)) # Crea la finestra
    pygame.display.set_caption("Visualizzatore Dati LiDAR (0 gradi in alto)") # Imposta il titolo della finestra

    # Chiedi all'utente il nome del file
    if len(sys.argv) > 1:
        data_filepath = sys.argv[1]
    else:
        
        data_filepath = "data.txt"
        print(f"Utilizzo del file di default: {data_filepath}. Puoi specificarlo come argomento: python {sys.argv[0]} [nome_file_dati]")

    lidar_points_polar, max_distance_found_in_file = load_lidar_data(data_filepath)
    
    if not lidar_points_polar:
        print("Nessun dato valido da visualizzare. Uscita.")
        pygame.quit()
        sys.exit()

    # --- CALCOLO DELLA SCALA CON MAX FISSO ---
    # Raggio massimo in pixel che i dati possono occupare senza andare fuori schermo
    # Usiamo un piccolo margine (es. 90% del raggio disponibile)
    max_display_radius = min(WIDTH, HEIGHT) // 2 * 0.9 

    # La distanza massima effettiva per la scala sarà il maggiore tra
    # la distanza massima trovata nel file e FIXED_MAX_DISTANCE_MM
    effective_max_distance_for_scale = min(max_distance_found_in_file, FIXED_MAX_DISTANCE_MM)

    # Calcola il fattore di scala: quanti pixel per ogni mm di distanza
    if effective_max_distance_for_scale > 0:
        SCALE_FACTOR = max_display_radius / effective_max_distance_for_scale
        print(f"Distanza massima trovata nel file: {max_distance_found_in_file:.2f} mm")
        print(f"Distanza massima effettiva per la scala: {effective_max_distance_for_scale:.2f} mm")
        print(f"Fattore di scala calcolato: {SCALE_FACTOR:.4f} pixel/mm")
    else:
        SCALE_FACTOR = 1.0 # Scala di default se non ci sono distanze valide
        print("Nessuna distanza valida trovata, usando fattore di scala di default.")


    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        screen.fill(BACKGROUND_COLOR) # Riempie lo sfondo

        # Disegna griglia e assi, passando la distanza massima effettiva per un migliore dimensionamento della griglia
        draw_grid_and_axes(screen, effective_max_distance_for_scale, SCALE_FACTOR) 

        # Disegna ogni punto del LiDAR
        for angle, distance in lidar_points_polar:
            if distance > 0.0:
                x_cartesian, y_cartesian = polar_to_cartesian(angle, distance, SCALE_FACTOR)
                # Disegna un cerchio per ogni punto.
                pygame.draw.circle(screen, POINT_COLOR, (x_cartesian, y_cartesian), 2) 

        pygame.display.flip() # Aggiorna lo schermo

    pygame.quit() # Deinizializza Pygame

if __name__ == "__main__":
    main()