import numpy as np
import matplotlib.pyplot as plt
import random
from matplotlib.colors import LinearSegmentedColormap
from PIL import Image

class CityDensityMapGenerator:
    
    def __init__(self, width=512, height=512, seed=None):
        """
        Initialisation 
        
        Args:
            width
            height
            seed
        """
        self.width = width
        self.height = height
        np.random.seed(seed)
    
    def generate_base_density(self, center_density=1.0, decay_rate=0.8):
        """
        Generate base density
        
        Args:
            center_density
            decay_rate
        """
        y, x = np.ogrid[:self.height, :self.width]
        center_y, center_x = self.height // 2, self.width // 2
        
        distance_from_center = np.sqrt((x - center_x)**2 + (y - center_y)**2)
        max_distance = np.sqrt(center_x**2 + center_y**2)
        normalized_distance = distance_from_center / max_distance
        
        density = center_density * np.exp(-decay_rate * normalized_distance)
        
        return density
    
    def add_urban_features(self, base_map, num_districts=5, road_width=3):
        """
        Add urban features as arteries and districts
        
        Args:
            base_map: Base map
            num_districts: Number of districts
            road_width: Width (no pixels)
        """
        density_map = base_map.copy()
        
        for _ in range(num_districts):
            district_center_x = np.random.randint(road_width, self.width - road_width)
            district_center_y = np.random.randint(road_width, self.height - road_width)
            
            district_size = np.random.randint(20, 60)
            district_density = np.random.uniform(0.4, 0.9)
            
            y, x = np.ogrid[:self.height, :self.width]
            mask = ((x - district_center_x)**2 + (y - district_center_y)**2) <= district_size**2
            density_map[mask] = np.maximum(density_map[mask], district_density)
        
        density_map[:, self.width//2-road_width:self.width//2+road_width] *= 1.2
        
        density_map[self.height//2-road_width:self.height//2+road_width, :] *= 1.2
        
        return np.clip(density_map, 0.0, 1.0)
    
    def add_noise(self, density_map, noise_scale=0.1):
        """
        Add noise for reality
        
        Args:
            density_map
            noise_scale
        """
        noise = np.random.normal(0, noise_scale, density_map.shape)
        return np.clip(density_map + noise, 0.0, 1.0)
    
    def generate_map(self, center_density=1.0, decay_rate=0.8, num_districts=5, noise_scale=0.1):
        """
        Generate density population map
        
        Args:
            center_density
            decay_rate
            num_districts
            noise_scale
        """
        base_density = self.generate_base_density(center_density, decay_rate)
        urban_density = self.add_urban_features(base_density, num_districts)
        final_density = self.add_noise(urban_density, noise_scale)
        
        return final_density
    
    def visualize(self, density_map, title="City Population Density Map", save_path=None):
        """
        Visualize
        
        Args:
            density_map: Mapa of density
            title: Title
            save_path: Path for save (optional)
        """
        plt.figure(figsize=(10, 8))

        # gray_r -> 0.0 = white, 1.0 = black
        im = plt.imshow(
            density_map,
            cmap='gray_r',      
            interpolation='bilinear',
            vmin=0.0,
            vmax=1.0
        )

        plt.colorbar(im, label='Population Density (0.0 = White, 1.0 = Black)')
        plt.title(title)
        plt.axis('off')

        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')

        plt.show()

    def save_as_bmp(self, density_map, bmp_path="city_density_map.bmp"):
        """
        Save the map as the BMP image.
        
        Args:
            density_map: array of values
            bmp_path: Path to BMP file
        """
        # Odwróć wartości: 0.0 (niska gęstość) -> 255 (biały)
        #                  1.0 (wysoka gęstość) -> 0 (czarny)
        inverted_density = 1.0 - density_map
        
        # Przeskaluj do 0-255 i konwertuj do uint8
        image_array = (inverted_density * 255).astype(np.uint8)
        
        # Utwórz obraz w skali szarości (tryb 'L')
        image = Image.fromarray(image_array, mode='L')
        
        # Zapisz jako BMP
        image.save(bmp_path, 'BMP')
        print(f"Map saved as BMP: {bmp_path}")
        
        return bmp_path


if __name__ == "__main__":
    generator = CityDensityMapGenerator(width=256, height=256, seed=random.randint(1, 100))
    
    density_map = generator.generate_map(
        center_density=1.0,    
        decay_rate=0.5,       
        num_districts=8,       
        noise_scale=0.05       
    )
    
    generator.visualize(density_map, title="Sample City Population Density Map")
    generator.save_as_bmp(density_map, bmp_path="city_density_map.bmp")
    
    print(f"Density map shape: {density_map.shape}")
    print(f"Min density: {density_map.min():.3f}")
    print(f"Max density: {density_map.max():.3f}")
    print(f"Mean density: {density_map.mean():.3f}")
