# lidar_subscriber.py
import zmq
import struct # Per la deserializzazione dei float
import time
import sys
import pygame # Per la visualizzazione
import math

# --- Configurazione Pygame ---
WIDTH, HEIGHT = 800, 800
CENTER_X, CENTER_Y = WIDTH // 2, HEIGHT // 2
BACKGROUND_COLOR = (0, 0, 0)
POINT_COLOR = (0, 255, 0)
AXIS_COLOR = (50, 50, 50)
GRID_COLOR = (20, 20, 20)
FIXED_MAX_DISTANCE_MM = 2000.0 # Deve corrispondere al tuo setup

# --- Funzioni di visualizzazione ---
def polar_to_cartesian(angle_degrees, distance_mm, scale):
    adjusted_angle_radians = math.radians(90 - angle_degrees)
    x = distance_mm * math.cos(adjusted_angle_radians) * scale
    y = distance_mm * math.sin(adjusted_angle_radians) * scale
    y = -y 
    display_x = int(x + CENTER_X)
    display_y = int(y + CENTER_Y)
    return display_x, display_y

def draw_grid_and_axes(screen, effective_max_distance_mm, scale_factor):
    max_screen_radius_pixels = min(WIDTH, HEIGHT) // 2
    grid_interval_mm = 1000 
    if effective_max_distance_mm < 1000:
        grid_interval_mm = 250 
    for r_mm in range(grid_interval_mm, int(effective_max_distance_mm * 1.1) + grid_interval_mm, grid_interval_mm): 
        radius_pixels = int(r_mm * scale_factor)
        if radius_pixels < max_screen_radius_pixels and radius_pixels > 0:
            pygame.draw.circle(screen, GRID_COLOR, (CENTER_X, CENTER_Y), radius_pixels, 1)
            font = pygame.font.Font(None, 15)
            text_surface = font.render(f"{r_mm/1000:.1f}m", True, GRID_COLOR)
            text_rect = text_surface.get_rect(center=(CENTER_X + radius_pixels + 15, CENTER_Y))
            screen.blit(text_surface, text_rect)

    pygame.draw.line(screen, AXIS_COLOR, (CENTER_X, 0), (CENTER_X, HEIGHT), 1) 
    pygame.draw.line(screen, AXIS_COLOR, (0, CENTER_Y), (WIDTH, CENTER_Y), 1)
    font = pygame.font.Font(None, 20)
    screen.blit(font.render("0° (+Y)", True, AXIS_COLOR), (CENTER_X + 5, 5)) 
    screen.blit(font.render("90° (+X)", True, AXIS_COLOR), (WIDTH - 40, CENTER_Y - 20))
    screen.blit(font.render("180° (-Y)", True, AXIS_COLOR), (CENTER_X + 5, HEIGHT - 25))
    screen.blit(font.render("270° (-X)", True, AXIS_COLOR), (5, CENTER_Y - 20))
# --- Fine funzioni di visualizzazione ---


def main():
    # Inizializza Pygame
    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Visualizzatore LiDAR in Tempo Reale (ZeroMQ)")

    # 1. Inizializza il contesto ZeroMQ
    context = zmq.Context()

    # 2. Crea un socket SUBSCRIBER
    subscriber = context.socket(zmq.SUB)
    # Connettiti all'indirizzo del publisher C++.
    # Deve essere lo stesso usato dal C++ con "bind".
    subscriber.connect("ipc:///tmp/lidar_data")
    # subscriber.connect("tcp://localhost:5556") # Alternativa per rete

    # 3. Iscriviti a tutti i messaggi (empty string significa tutti i topic)
    subscriber.setsockopt_string(zmq.SUBSCRIBE, "")

    print("Subscriber Python avviato. In attesa di dati LiDAR...")

    current_lidar_points = []
    
    # Calcolo iniziale della scala (basato su FIXED_MAX_DISTANCE_MM)
    max_display_radius = min(WIDTH, HEIGHT) // 2 * 0.9
    SCALE_FACTOR = max_display_radius / FIXED_MAX_DISTANCE_MM
    print(f"Scala impostata su: {SCALE_FACTOR:.4f} pixel/mm (basata su {FIXED_MAX_DISTANCE_MM}mm max)")

    running = True
    count = 0
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE or event.key == pygame.K_Q:
                    running = False

        try:
            # 4. Ricevi il messaggio non bloccante per non bloccare Pygame
            # Utilizza una timeout per pygame event loop
            message = subscriber.recv(zmq.NOBLOCK)
            
            # Se un messaggio è stato ricevuto, deserializzalo
            if message:
                current_lidar_points = []
                # 'f' è per float, 2 è per coppia (theta, distance)
                # Calcola quanti float ci sono nel messaggio
                num_floats = len(message) // struct.calcsize('f') 
                # Il formato stringa per unpack è 'ff' ripetuto num_floats / 2 volte
                format_string = f'{num_floats}f'
                
                # Deserializza l'intero buffer in una tupla di float
                unpacked_data = struct.unpack(format_string, message)
                
                # Raggruppa i float in coppie (theta, distance)
                for i in range(0, len(unpacked_data), 2):
                    theta = unpacked_data[i]
                    distance = unpacked_data[i+1]
                    current_lidar_points.append((theta, distance))
                
                count += 1
                print(f"Ricevuti {len(current_lidar_points)} punti ({count}).")

        except zmq.Again:
            # Nessun messaggio disponibile, continua con il rendering della scansione precedente
            pass
        except Exception as e:
            print(f"Errore durante la ricezione o deserializzazione: {e}")
            running = False # Esci in caso di errore grave

        screen.fill(BACKGROUND_COLOR)
        draw_grid_and_axes(screen, FIXED_MAX_DISTANCE_MM, SCALE_FACTOR)

        # Disegna i punti dell'ultima scansione ricevuta
        for angle, distance in current_lidar_points:
            x_cartesian, y_cartesian = polar_to_cartesian(angle, distance, SCALE_FACTOR)
            pygame.draw.circle(screen, POINT_COLOR, (x_cartesian, y_cartesian), 2)
            #pygame.draw.line(screen, POINT_COLOR, (CENTER_X, CENTER_Y), (x_cartesian, y_cartesian), 1)

        pygame.display.flip()

    pygame.quit()
    print("Programma terminato.")

if __name__ == "__main__":
    main()