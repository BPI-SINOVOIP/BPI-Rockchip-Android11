function alloc(sizeMB, randomRatio) {
  const FLOAT64_BYTES = 8;
  const MB = 1024 * 1024;
  const total_count = sizeMB* MB / FLOAT64_BYTES;
  const random_count = total_count * randomRatio;
  // Random array is uncompressable.
  let random_array = new Float64Array(random_count);
  for (let i = 0; i < random_array.length; i++) {
    random_array[i] = Math.random();
  }
  // Constant array is compressable.
  const const_count = total_count * (1 - randomRatio);
  let const_array = new Float64Array(const_count);
  for (let i = 0; i < const_array.length; i++) {
    const_array[i] = 1;
  }
  return [random_array, const_array];
}
$(document).ready(function() {
  var url = new URL(window.location.href);
  var allocMB = parseInt(url.searchParams.get("alloc"));
  if (isNaN(allocMB))
    allocMB = 800;
  var randomRatio = parseFloat(url.searchParams.get("ratio"));
  if (isNaN(randomRatio))
    randomRatio = 0.666

  var startTime = new Date();
  // Assigns the content to docuement to avoid optimization of unused data.
  document.out = alloc(allocMB, randomRatio);
  var ellapse = (new Date() - startTime) / 1000;
  // Shows the loading time for manual test.
  $("#display").text(`Allocating ${allocMB} MB takes ${ellapse} seconds`);
});
